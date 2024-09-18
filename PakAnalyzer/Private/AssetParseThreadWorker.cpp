#include "AssetParseThreadWorker.h"

#include "Async/ParallelFor.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "IPlatformFilePak.h"
#include "Misc/Compression.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/ObjectResource.h"
#include "UObject/ObjectVersion.h"
#include "UObject/PackageFileSummary.h"

#include "CommonDefines.h"
#include "ExtractThreadWorker.h"

class FAssetParseMemoryReader : public FMemoryReader
{
public:
	explicit FAssetParseMemoryReader(const TArray<FNameEntryId>& InNameMap, const TArray<uint8>& InBytes, bool bIsPersistent = false)
		: FMemoryReader(InBytes, bIsPersistent)
		, NameMap(InNameMap)
	{
	}

	FArchive& operator<<(FName& InName)
	{
		FArchive& Ar = *this;
		int32 NameIndex;
		Ar << NameIndex;
		int32 Number = 0;
		Ar << Number;

		if (NameMap.IsValidIndex(NameIndex))
		{
			// if the name wasn't loaded (because it wasn't valid in this context)
			FNameEntryId MappedName = NameMap[NameIndex];

			// simply create the name from the NameMap's name and the serialized instance number
			InName = FName::CreateFromDisplayId(MappedName, Number);
		}

		return Ar;
	}

protected:
	const TArray<FNameEntryId>& NameMap;
};

template<class T>
FString FindFullPath(const TArray<T>& InMaps, int32 Index, const FString& InPathSpliter = TEXT("/"))
{
	if (!InMaps.IsValidIndex(Index))
	{
		return TEXT("Invalid");
	}

	const T& Object = InMaps[Index];
	FString ObjectPath = Object.ObjectName.ToString();

	if (!Object.OuterIndex.IsNull())
	{
		const FString ParentObjectPath = FindFullPath(InMaps, Object.OuterIndex.IsImport() ? Object.OuterIndex.ToImport() : Object.OuterIndex.ToExport(), InPathSpliter);
		ObjectPath = ParentObjectPath + InPathSpliter + ObjectPath;
	}

	return ObjectPath;
}

FAssetParseThreadWorker::FAssetParseThreadWorker()
	: Thread(nullptr)
{
}

FAssetParseThreadWorker::~FAssetParseThreadWorker()
{
	Shutdown();
}

bool FAssetParseThreadWorker::Init()
{
	return true;
}

uint32 FAssetParseThreadWorker::Run()
{
	UE_LOG(LogPakAnalyzer, Display, TEXT("Asset parse worker starts."));

	FCriticalSection Mutex;
	const static bool bForceSingleThread = false;
	const int32 TotalCount = Files.Num();
	
	TMultiMap<FName, FName> DependsMap;
	TMap<FName, FName> ClassMap;

	// Parse assets
	ParallelFor(TotalCount, [this, &DependsMap, &ClassMap, &Mutex](int32 InIndex){
		if (StopTaskCounter.GetValue() > 0)
		{
			return;
		}

		TArray<uint8> FileBuffer;
		bool SerializeSuccess = false;

		FPakFileEntryPtr File = Files[InIndex];
		if (!Summaries.IsValidIndex(File->OwnerPakIndex) || File->PakEntry.IsDeleteRecord())
		{
			return;
		}

		const FPakFileSumary& Summary = Summaries[File->OwnerPakIndex];
		const FString PakFilePath = Summary.PakFilePath;
		const int32 PakVersion = Summary.PakInfo.Version;
		const FAES::FAESKey AESKey = Summary.DecryptAESKey;

		if (OnReadAssetContent.IsBound())
		{
			OnReadAssetContent.Execute(File, SerializeSuccess, FileBuffer);
		}
		else
		{
			FArchive* ReaderArchive = IFileManager::Get().CreateFileReader(*PakFilePath);
			ReaderArchive->Seek(File->PakEntry.Offset);

			FPakEntry EntryInfo;
			EntryInfo.Serialize(*ReaderArchive, PakVersion);

			if (EntryInfo.IndexDataEquals(File->PakEntry))
			{
				FMemoryWriter Writer(FileBuffer, false, true);

				if (EntryInfo.CompressionMethodIndex == 0)
				{
					const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
					void* Buffer = FMemory::Malloc(BufferSize);

					if (FExtractThreadWorker::BufferedCopyFile(Writer, *ReaderArchive, File->PakEntry, Buffer, BufferSize, AESKey))
					{
						SerializeSuccess = true;
					}

					FMemory::Free(Buffer);
				}
				else
				{
					uint8* PersistantCompressionBuffer = NULL;
					int64 CompressionBufferSize = 0;
					const bool bHasRelativeCompressedChunkOffsets = PakVersion >= FPakInfo::PakFile_Version_RelativeChunkOffsets;

					if (FExtractThreadWorker::UncompressCopyFile(Writer, *ReaderArchive, File->PakEntry, PersistantCompressionBuffer, CompressionBufferSize, AESKey, File->CompressionMethod, bHasRelativeCompressedChunkOffsets))
					{
						SerializeSuccess = true;
					}

					FMemory::Free(PersistantCompressionBuffer);
				}
			}

			ReaderArchive->Close();
			delete ReaderArchive;
		}

		if (SerializeSuccess)
		{
			if (!File->AssetSummary.IsValid())
			{
				File->AssetSummary = MakeShared<FAssetSummary>();
			}
			else
			{
				File->AssetSummary->Names.Empty();
				File->AssetSummary->ObjectExports.Empty();
				File->AssetSummary->ObjectImports.Empty();
			}
			
			TArray<FNameEntryId> NameMap;
			FAssetParseMemoryReader Reader(NameMap, FileBuffer);

			// Serialize summary
			Reader << File->AssetSummary->PackageSummary;
			
			Reader.Seek(0);
			int32 Tag = 0;
			Reader << Tag;
			if (Tag == PACKAGE_FILE_TAG_SWAPPED)
			{
				if (Reader.ForceByteSwapping())
				{
					Reader.SetByteSwapping(false);
				}
				else
				{
					Reader.SetByteSwapping(true);
				}
			}

			int32 LegacyFileVersion = -8;
			Reader << LegacyFileVersion;

			if (LegacyFileVersion >= -7)
			{
				// UE4 pak
				Reader.SetUEVer(FPackageFileVersion(VER_LATEST_ENGINE_UE4, EUnrealEngineObjectUE5Version::INITIAL_VERSION));
			}
			
			// Serialize Names
			const int32 NameCount = File->AssetSummary->PackageSummary.NameCount;
			if (NameCount > 0)
			{
				NameMap.Reserve(NameCount);
			}

			FNameEntrySerialized NameEntry(ENAME_LinkerConstructor);
			Reader.Seek(File->AssetSummary->PackageSummary.NameOffset);

			for (int32 i = 0; i < NameCount; ++i)
			{
				Reader << NameEntry;
				NameMap.Emplace(FName(NameEntry).GetDisplayIndex());

				if (NameEntry.bIsWide)
				{
					File->AssetSummary->Names.Add(MakeShared<FName>(NameEntry.WideName));
				}
				else
				{
					File->AssetSummary->Names.Add(MakeShared<FName>(NameEntry.AnsiName));
				}
			}
			File->AssetSummary->Names.Shrink();

			// Serialize Export Table
			TArray<FObjectExport> Exports;
			Exports.AddZeroed(File->AssetSummary->PackageSummary.ExportCount);
			Reader.Seek(File->AssetSummary->PackageSummary.ExportOffset);
			for (int32 i = 0; i < File->AssetSummary->PackageSummary.ExportCount; ++i)
			{
				Reader << Exports[i];

				FObjectExportPtrType ExportEx = MakeShared<FObjectExportEx>();
				ExportEx->Index = i;
				ExportEx->ObjectName = Exports[i].ObjectName;
				ExportEx->SerialSize = Exports[i].SerialSize;
				ExportEx->SerialOffset = Exports[i].SerialOffset;
				ExportEx->bIsAsset = Exports[i].bIsAsset;
				ExportEx->bNotForClient = Exports[i].bNotForClient;
				ExportEx->bNotForServer = Exports[i].bNotForServer;

				File->AssetSummary->ObjectExports.Add(ExportEx);
			}
			File->AssetSummary->ObjectExports.Shrink();

			// Serialize Import Table
			TArray<FObjectImport> Imports;
			Imports.AddZeroed(File->AssetSummary->PackageSummary.ImportCount);
			Reader.Seek(File->AssetSummary->PackageSummary.ImportOffset);
			for (int32 i = 0; i < File->AssetSummary->PackageSummary.ImportCount; ++i)
			{
				Reader << Imports[i];

				FObjectImportPtrType ImportEx = MakeShared<FObjectImportEx>();
				ImportEx->Index = i;
				ImportEx->ObjectName = Imports[i].ObjectName;
				ImportEx->ClassPackage = Imports[i].ClassPackage;
				ImportEx->ClassName = Imports[i].ClassName;

				File->AssetSummary->ObjectImports.Add(ImportEx);
			}
			File->AssetSummary->ObjectImports.Shrink();

			FName MainObjectName = *FPaths::GetBaseFilename(File->Filename.ToString());
			FName MainClassObjectName = *FString::Printf(TEXT("%s_C"), *MainObjectName.ToString());
			FName MainObjectClassName = NAME_None;
			FName MainClassObjectClassName = NAME_None;
			FName AssetClass = NAME_None;

			// Parse Export Object Path
			for (int32 i = 0; i < File->AssetSummary->ObjectExports.Num(); ++i)
			{
				const FObjectExport& Export = Exports[i];
				FObjectExportPtrType& ExportEx = File->AssetSummary->ObjectExports[i];
				ExportEx->ObjectPath = *FindFullPath(Exports, i, TEXT("."));

				ParseObjectName(Imports, Exports, Export.ClassIndex, ExportEx->ClassName);
				ParseObjectName(Imports, Exports, Export.TemplateIndex, ExportEx->TemplateObject);
				ParseObjectName(Imports, Exports, Export.SuperIndex, ExportEx->Super);

				FName ObjectName = *FPaths::GetBaseFilename(ExportEx->ObjectName.ToString());
				if (ObjectName == MainObjectName)
				{
					MainObjectClassName = ExportEx->ClassName;
				}
				else if (ObjectName == MainClassObjectName)
				{
					MainClassObjectClassName = ExportEx->ClassName;
				}

				if (ExportEx->bIsAsset)
				{
					AssetClass = ExportEx->ClassName;
				}
			}

			if (MainObjectClassName == NAME_None && MainClassObjectClassName == NAME_None)
			{
				if (File->AssetSummary->ObjectExports.Num() == 1)
				{
					MainObjectClassName = File->AssetSummary->ObjectExports[0]->ClassName;
				}
				else if (!AssetClass.IsNone())
				{
					MainObjectClassName = AssetClass;
				}
			}

			if (MainObjectClassName != NAME_None || MainClassObjectClassName != NAME_None)
			{
				FScopeLock ScopeLock(&Mutex);
				ClassMap.Add(File->PackagePath, MainObjectClassName != NAME_None ? MainObjectClassName : MainClassObjectClassName);
			}

			const bool bFillDependency = File->AssetSummary->DependencyList.Num() <= 0;
			for (int32 i = 0; i < File->AssetSummary->ObjectImports.Num(); ++i)
			{
				const FObjectImport& Import = Imports[i];
				FObjectImportPtrType& ImportEx = File->AssetSummary->ObjectImports[i];

				ImportEx->ObjectPath = *FindFullPath(Imports, i);

				if (bFillDependency && Import.ClassName == "Package" && !ImportEx->ObjectPath.ToString().StartsWith(TEXT("/Script")))
				{
					FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
					Depends->PackageName = ImportEx->ObjectPath;
					File->AssetSummary->DependencyList.Add(Depends);

					FScopeLock ScopeLock(&Mutex);
					DependsMap.Add(ImportEx->ObjectPath, File->PackagePath);
				}
			}
			File->AssetSummary->DependencyList.Shrink();

			// Serialize Preload Dependency
			TArray<FPackageIndex> PreloadDependencies;
			if (File->AssetSummary->PackageSummary.PreloadDependencyCount > 0)
			{
				PreloadDependencies.AddZeroed(File->AssetSummary->PackageSummary.PreloadDependencyCount);
				Reader.Seek(File->AssetSummary->PackageSummary.PreloadDependencyOffset);
				for (int32 i = 0; i < File->AssetSummary->PackageSummary.PreloadDependencyCount; ++i)
				{
					Reader << PreloadDependencies[i];
				}

				// Parse Preload Dependency
				for (int32 i = 0; i < File->AssetSummary->ObjectExports.Num(); ++i)
				{
					const FObjectExport& Export = Exports[i];
					FObjectExportPtrType& ExportEx = File->AssetSummary->ObjectExports[i];

					if (Export.FirstExportDependency >= 0)
					{
						FName ObjectName;
						int32 RunningIndex = Export.FirstExportDependency;
						for (int32 Index = Export.SerializationBeforeSerializationDependencies; Index > 0; Index--)
						{
							FPackageIndex Dep = PreloadDependencies[RunningIndex++];

							if (ParseObjectPath(File->AssetSummary, Dep, ObjectName))
							{
								FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
								Depends->PackageName = ObjectName;
								Depends->ExtraInfo = TEXT("Serialization Before Serialization");

								ExportEx->DependencyList.Add(Depends);
							}
						}

						for (int32 Index = Export.CreateBeforeSerializationDependencies; Index > 0; Index--)
						{
							FPackageIndex Dep = PreloadDependencies[RunningIndex++];

							if (ParseObjectPath(File->AssetSummary, Dep, ObjectName))
							{
								FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
								Depends->PackageName = ObjectName;
								Depends->ExtraInfo = TEXT("Create Before Serialization");

								ExportEx->DependencyList.Add(Depends);
							}
						}

						for (int32 Index = Export.SerializationBeforeCreateDependencies; Index > 0; Index--)
						{
							FPackageIndex Dep = PreloadDependencies[RunningIndex++];

							if (ParseObjectPath(File->AssetSummary, Dep, ObjectName))
							{
								FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
								Depends->PackageName = ObjectName;
								Depends->ExtraInfo = TEXT("Serialization Before Create");

								ExportEx->DependencyList.Add(Depends);
							}
						}

						for (int32 Index = Export.CreateBeforeCreateDependencies; Index > 0; Index--)
						{
							FPackageIndex Dep = PreloadDependencies[RunningIndex++];

							if (ParseObjectPath(File->AssetSummary, Dep, ObjectName))
							{
								FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
								Depends->PackageName = ObjectName;
								Depends->ExtraInfo = TEXT("Create Before Create");

								ExportEx->DependencyList.Add(Depends);
							}
						}

						ExportEx->DependencyList.Shrink();
					}
				}
			}
		}
	}, bForceSingleThread);

	// Parse depends
	ParallelFor(TotalCount, [this, &DependsMap](int32 InIndex) {
		if (StopTaskCounter.GetValue() > 0)
		{
			return;
		}

		FPakFileEntryPtr File = Files[InIndex];
		if (!File->AssetSummary.IsValid())
		{
			return;
		}
		if (File->AssetSummary->DependentList.Num() > 0)
		{
			return;
		}

		TArray<FName> Assets;
		DependsMap.MultiFind(File->PackagePath, Assets);

		for (const FName& Asset : Assets)
		{
			FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
			Depends->PackageName = Asset;
			File->AssetSummary->DependentList.Add(Depends);
		}
		File->AssetSummary->DependentList.Shrink();
	}, bForceSingleThread);

	OnParseFinish.ExecuteIfBound(StopTaskCounter.GetValue() > 0, ClassMap);

	StopTaskCounter.Reset();

	UE_LOG(LogPakAnalyzer, Display, TEXT("Asset parse worker exits."));

	return 0;
}

void FAssetParseThreadWorker::Stop()
{
	StopTaskCounter.Increment();
	EnsureCompletion();
	StopTaskCounter.Reset();
}

void FAssetParseThreadWorker::Exit()
{

}

void FAssetParseThreadWorker::Shutdown()
{
	Stop();

	if (Thread)
	{
		UE_LOG(LogPakAnalyzer, Log, TEXT("Shutdown asset parse worker."));

		delete Thread;
		Thread = nullptr;
	}
}

void FAssetParseThreadWorker::EnsureCompletion()
{
	if (Thread)
	{
		Thread->WaitForCompletion();
	}
}

void FAssetParseThreadWorker::StartParse(TArray<FPakFileEntryPtr>& InFiles, TArray<FPakFileSumary>& InSummaries)
{
	Shutdown();

	UE_LOG(LogPakAnalyzer, Log, TEXT("Start asset parse worker, file count: %d."), InFiles.Num());

	Files = MoveTemp(InFiles);
	Summaries = MoveTemp(InSummaries);

	Thread = FRunnableThread::Create(this, TEXT("AssetParseThreadWorker"), 0, EThreadPriority::TPri_Highest);
}

bool FAssetParseThreadWorker::ParseObjectName(const TArray<FObjectImport>& Imports, const TArray<FObjectExport>& Exports, FPackageIndex Index, FName& OutObjectName)
{
	if (Index.IsImport())
	{
		const int32 RawIndex = Index.ToImport();
		if (Imports.IsValidIndex(RawIndex))
		{
			OutObjectName = Imports[RawIndex].ObjectName;
			return true;
		}
	}
	else if (Index.IsExport())
	{
		const int32 RawIndex = Index.ToExport();
		if (Exports.IsValidIndex(RawIndex))
		{
			OutObjectName = Exports[RawIndex].ObjectName;
			return true;
		}
	}

	return false;
}

bool FAssetParseThreadWorker::ParseObjectPath(FAssetSummaryPtr InSummary, FPackageIndex Index, FName& OutFullPath)
{
	if (Index.IsImport())
	{
		const int32 RawIndex = Index.ToImport();
		if (InSummary->ObjectImports.IsValidIndex(RawIndex))
		{
			OutFullPath = InSummary->ObjectImports[RawIndex]->ObjectPath;
			return true;
		}
	}
	else if (Index.IsExport())
	{
		const int32 RawIndex = Index.ToExport();
		if (InSummary->ObjectExports.IsValidIndex(RawIndex))
		{
			OutFullPath = InSummary->ObjectExports[RawIndex]->ObjectPath;
			return true;
		}
	}

	return false;
}
