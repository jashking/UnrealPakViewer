#include "AssetParseThreadWorker.h"

#include "Async/ParallelFor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "IPlatformFilePak.h"
#include "Misc/Compression.h"
#include "Misc/Paths.h"
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
	const int32 TotalCount = Files.Num();

	ParallelFor(TotalCount, [this](int32 InIndex){
		if (StopTaskCounter.GetValue() > 0)
		{
			return;
		}

		FPakFileEntryPtr File = Files[InIndex];

		FArchive* ReaderArchive = IFileManager::Get().CreateFileReader(*PakFilePath);
		ReaderArchive->Seek(File->PakEntry.Offset);

		FPakEntry EntryInfo;
		EntryInfo.Serialize(*ReaderArchive, PakVersion);

		if (EntryInfo.IndexDataEquals(File->PakEntry))
		{
			bool SerializeSuccess = false;

			TArray<uint8> FileBuffer;
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

					if (Export->ClassIndex.IsImport())
					{
						const int32 ClassIndex = Export->ClassIndex.ToImport();
						if (File->AssetSummary->ObjectImports.IsValidIndex(ClassIndex))
						{
							Export->ClassName = File->AssetSummary->ObjectImports[ClassIndex]->ObjectName.ToString();
						}
					}
					else if (Export->ClassIndex.IsExport())
					{
						const int32 ClassIndex = Export->ClassIndex.ToExport();
						if (File->AssetSummary->ObjectExports.IsValidIndex(ClassIndex))
						{
							Export->ClassName = File->AssetSummary->ObjectExports[ClassIndex]->ObjectName.ToString();
						}
					}

					if (Export->TemplateIndex.IsImport())
					{
						const int32 TemplateIndex = Export->TemplateIndex.ToImport();
						if (File->AssetSummary->ObjectImports.IsValidIndex(TemplateIndex))
						{
							Export->TemplateObject = File->AssetSummary->ObjectImports[TemplateIndex]->ObjectName.ToString();
						}
					}
					else if (Export->TemplateIndex.IsExport())
					{
						const int32 TemplateIndex = Export->TemplateIndex.ToExport();
						if (File->AssetSummary->ObjectExports.IsValidIndex(TemplateIndex))
						{
							Export->TemplateObject = File->AssetSummary->ObjectExports[TemplateIndex]->ObjectName.ToString();
						}
					}

					if (Export->SuperIndex.IsImport())
					{
						const int32 SuperIndex = Export->SuperIndex.ToImport();
						if (File->AssetSummary->ObjectImports.IsValidIndex(SuperIndex))
						{
							Export->Super = File->AssetSummary->ObjectImports[SuperIndex]->ObjectName.ToString();
						}
					}
					else if (Export->SuperIndex.IsExport())
					{
						const int32 SuperIndex = Export->SuperIndex.ToExport();
						if (File->AssetSummary->ObjectExports.IsValidIndex(SuperIndex))
						{
							Export->Super = File->AssetSummary->ObjectExports[SuperIndex]->ObjectName.ToString();
						}
					}
				}

				for (int32 i = 0; i < File->AssetSummary->ObjectImports.Num(); ++i)
				{
					File->AssetSummary->ObjectImports[i]->ObjectPath = FindFullPath(File->AssetSummary->ObjectImports, i);
				}

				// Serialize Preload Dependency
				Reader.Seek(File->AssetSummary->PackageSummary.PreloadDependencyOffset);
				for (int32 i = 0; i < File->AssetSummary->PackageSummary.PreloadDependencyCount; ++i)
				{
					FPackageIndexPtrType Dependency = MakeShared<FPackageIndex>();
					Reader << *(Dependency.Get());

					File->AssetSummary->PreloadDependency.Add(Dependency);
				}
			}
		}

		ReaderArchive->Close();
		delete ReaderArchive;
	}, false);

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
