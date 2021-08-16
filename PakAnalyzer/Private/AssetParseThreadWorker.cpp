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
	FString ObjectPath = Object->ObjectName.ToString();

	if (!Object->OuterIndex.IsNull())
	{
		const FString ParentObjectPath = FindFullPath(InMaps, Object->OuterIndex.IsImport() ? Object->OuterIndex.ToImport() : Object->OuterIndex.ToExport(), InPathSpliter);
		ObjectPath = ParentObjectPath + InPathSpliter + ObjectPath;
	}

	return ObjectPath;
}

FAssetParseThreadWorker::FAssetParseThreadWorker()
	: Thread(nullptr)
	, PakVersion(FPakInfo::PakFile_Version_Latest)
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
	FCriticalSection Mutex;
	const static bool bForceSingleThread = false;
	const int32 TotalCount = Files.Num();
	
	TMultiMap<FString, FString> DependsMap;

	// Parse assets
	ParallelFor(TotalCount, [this, &DependsMap, &Mutex](int32 InIndex){
		if (StopTaskCounter.GetValue() > 0)
		{
			return;
		}

		TArray<uint8> FileBuffer;
		bool SerializeSuccess = false;

		FPakFileEntryPtr File = Files[InIndex];

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
				File->AssetSummary->PreloadDependency.Empty();
			}

			TArray<FNameEntryId> NameMap;
			FAssetParseMemoryReader Reader(NameMap, FileBuffer);

			// Serialize summary
			Reader << File->AssetSummary->PackageSummary;

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

			// Serialize Export Table
			Reader.Seek(File->AssetSummary->PackageSummary.ExportOffset);
			for (int32 i = 0; i < File->AssetSummary->PackageSummary.ExportCount; ++i)
			{
				FObjectExportPtrType Export = MakeShared<FObjectExportEx>();
				Reader << *(Export.Get());

				Export->Index = i;

				File->AssetSummary->ObjectExports.Add(Export);
			}

			// Serialize Import Table
			Reader.Seek(File->AssetSummary->PackageSummary.ImportOffset);
			for (int32 i = 0; i < File->AssetSummary->PackageSummary.ImportCount; ++i)
			{
				FObjectImportPtrType Import = MakeShared<FObjectImportEx>();
				Reader << *(Import.Get());

				Import->Index = i;

				File->AssetSummary->ObjectImports.Add(Import);
			}

			// Parse Export Object Path
			for (int32 i = 0; i < File->AssetSummary->ObjectExports.Num(); ++i)
			{
				FObjectExportPtrType& Export = File->AssetSummary->ObjectExports[i];
				Export->ObjectPath = FindFullPath(File->AssetSummary->ObjectExports, i, TEXT("."));

				ParseObjectName(File->AssetSummary, Export->ClassIndex, Export->ClassName);
				ParseObjectName(File->AssetSummary, Export->TemplateIndex, Export->TemplateObject);
				ParseObjectName(File->AssetSummary, Export->SuperIndex, Export->Super);
			}

			for (int32 i = 0; i < File->AssetSummary->ObjectImports.Num(); ++i)
			{
				FObjectImportPtrType& Import = File->AssetSummary->ObjectImports[i];

				Import->ObjectPath = FindFullPath(File->AssetSummary->ObjectImports, i);

				if (Import->ClassName == "Package" && !Import->ObjectPath.StartsWith(TEXT("/Script")))
				{
					FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
					Depends->PackageName = Import->ObjectPath;
					File->AssetSummary->DependencyList.Add(Depends);

					FScopeLock ScopeLock(&Mutex);
					DependsMap.Add(Import->ObjectPath.ToLower(), File->PackagePath);
				}
			}

			// Serialize Preload Dependency
			Reader.Seek(File->AssetSummary->PackageSummary.PreloadDependencyOffset);
			for (int32 i = 0; i < File->AssetSummary->PackageSummary.PreloadDependencyCount; ++i)
			{
				FPackageIndexPtrType Dependency = MakeShared<FPackageIndex>();
				Reader << *(Dependency.Get());

				File->AssetSummary->PreloadDependency.Add(Dependency);
			}

			// Parse Preload Dependency
			for (int32 i = 0; i < File->AssetSummary->ObjectExports.Num(); ++i)
			{
				FObjectExportPtrType& Export = File->AssetSummary->ObjectExports[i];
				TArray<FPackageIndexPtrType>& PreloadDependencies = File->AssetSummary->PreloadDependency;

				if (Export->FirstExportDependency >= 0)
				{
					FString ObjectName;
					int32 RunningIndex = Export->FirstExportDependency;
					for (int32 Index = Export->SerializationBeforeSerializationDependencies; Index > 0; Index--)
					{
						FPackageIndexPtrType Dep = PreloadDependencies[RunningIndex++];

						if (ParseObjectPath(File->AssetSummary, *Dep, ObjectName))
						{
							FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
							Depends->PackageName = ObjectName;
							Depends->ExtraInfo = TEXT("Serialization Before Serialization");

							Export->DependencyList.Add(Depends);
						}
					}

					for (int32 Index = Export->CreateBeforeSerializationDependencies; Index > 0; Index--)
					{
						FPackageIndexPtrType Dep = PreloadDependencies[RunningIndex++];

						if (ParseObjectPath(File->AssetSummary, *Dep, ObjectName))
						{
							FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
							Depends->PackageName = ObjectName;
							Depends->ExtraInfo = TEXT("Create Before Serialization");

							Export->DependencyList.Add(Depends);
						}
					}

					for (int32 Index = Export->SerializationBeforeCreateDependencies; Index > 0; Index--)
					{
						FPackageIndexPtrType Dep = PreloadDependencies[RunningIndex++];

						if (ParseObjectPath(File->AssetSummary, *Dep, ObjectName))
						{
							FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
							Depends->PackageName = ObjectName;
							Depends->ExtraInfo = TEXT("Serialization Before Create");

							Export->DependencyList.Add(Depends);
						}
					}

					for (int32 Index = Export->CreateBeforeCreateDependencies; Index > 0; Index--)
					{
						FPackageIndexPtrType Dep = PreloadDependencies[RunningIndex++];

						if (ParseObjectPath(File->AssetSummary, *Dep, ObjectName))
						{
							FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
							Depends->PackageName = ObjectName;
							Depends->ExtraInfo = TEXT("Create Before Create");

							Export->DependencyList.Add(Depends);
						}
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

		TArray<FString> Assets;
		DependsMap.MultiFind(File->PackagePath.ToLower(), Assets);

		for (const FString& Asset : Assets)
		{
			FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
			Depends->PackageName = Asset;
			File->AssetSummary->DependentList.Add(Depends);
		}
	}, bForceSingleThread);

	StopTaskCounter.Reset();
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

void FAssetParseThreadWorker::StartParse(TArray<FPakFileEntryPtr>& InFiles, const FString& InPakFile, int32 InPakVersion, const FAES::FAESKey& InKey)
{
	Shutdown();

	UE_LOG(LogPakAnalyzer, Log, TEXT("Start asset parse worker, file count: %d, pak version: %d."), InFiles.Num(), InPakVersion);

	Files = MoveTemp(InFiles);
	PakFilePath = InPakFile;
	PakVersion = InPakVersion;
	AESKey = InKey;

	Thread = FRunnableThread::Create(this, TEXT("AssetParseThreadWorker"), 0, EThreadPriority::TPri_Highest);
}

bool FAssetParseThreadWorker::ParseObjectName(FAssetSummaryPtr InSummary, FPackageIndex Index, FString& OutObjectName)
{
	if (Index.IsImport())
	{
		const int32 RawIndex = Index.ToImport();
		if (InSummary->ObjectImports.IsValidIndex(RawIndex))
		{
			OutObjectName = InSummary->ObjectImports[RawIndex]->ObjectName.ToString();
			return true;
		}
	}
	else if (Index.IsExport())
	{
		const int32 RawIndex = Index.ToExport();
		if (InSummary->ObjectExports.IsValidIndex(RawIndex))
		{
			OutObjectName = InSummary->ObjectExports[RawIndex]->ObjectName.ToString();
			return true;
		}
	}

	return false;
}

bool FAssetParseThreadWorker::ParseObjectPath(FAssetSummaryPtr InSummary, FPackageIndex Index, FString& OutFullPath)
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
