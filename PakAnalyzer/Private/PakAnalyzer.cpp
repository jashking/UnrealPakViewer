// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PakAnalyzer.h"

#include "ARFilter.h"
#include "AssetData.h"
#include "AssetRegistryState.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformMisc.h"
#include "Json.h"
#include "Launch/Resources/Version.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryWriter.h"

#include "CommonDefines.h"
#include "ExtractThreadWorker.h"

#if ENGINE_MINOR_VERSION >= 26
typedef FPakFile::FPakEntryIterator RecordIterator;
#else
typedef FPakFile::FFileIterator RecordIterator;
#endif

FPakAnalyzer::FPakAnalyzer()
	: ExtractWorkerCount(DEFAULT_EXTRACT_THREAD_COUNT)
{
	Reset();
	InitializeExtractWorker();
}

FPakAnalyzer::~FPakAnalyzer()
{
	ShutdownAllExtractWorker();
	Reset();
}

bool FPakAnalyzer::LoadPakFile(const FString& InPakPath)
{
	if (InPakPath.IsEmpty())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Pak path is empty!"));
		return false;
	}

	UE_LOG(LogPakAnalyzer, Log, TEXT("Start load pak file: %s."), *InPakPath);

	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	if (!PlatformFile.FileExists(*InPakPath))
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Load pak file failed! Pak file not exists! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Pak file not exists! Path: %s."), *InPakPath);
		return false;
	}

	Reset();

	if (!PreLoadPak(InPakPath))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Pre load pak file failed! Path: %s."), *InPakPath);
		return false;
	}

#if ENGINE_MINOR_VERSION >= 27
	TRefCountPtr<FPakFile> PakFile = new FPakFile(*InPakPath, false);
	FPakFile* PakFilePtr = PakFile.GetReference();
#else
	TSharedPtr<FPakFile> PakFile = MakeShared<FPakFile>(*InPakPath, false);
	FPakFile* PakFilePtr = PakFile.Get();
#endif // ENGINE_MINOR_VERSION >= 27
	if (!PakFilePtr)
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Load pak file failed! Create PakFile failed! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Create PakFile failed! Path: %s."), *InPakPath);

		return false;
	}

	if (!PakFilePtr->IsValid())
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Load pak file failed! Unable to open pak file! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Unable to open pak file! Path: %s."), *InPakPath);

		return false;
	}

	// Generate unique id
	LoadGuid = FGuid::NewGuid();

	// Save pak sumary
	PakFileSumary.MountPoint = PakFilePtr->GetMountPoint();
	PakFileSumary.PakInfo = PakFilePtr->GetInfo();
	PakFileSumary.PakFilePath = InPakPath;
	PakFileSumary.PakFileSize = PakFilePtr->TotalSize();

	TArray<FString> Methods;
	for (const FName& Name : PakFileSumary.PakInfo.CompressionMethods)
	{
		Methods.Add(Name.ToString());
	}
	PakFileSumary.CompressionMethods = FString::Join(Methods, TEXT(", "));

	// Make tree root
	TreeRoot = MakeShared<FPakTreeEntry>(FPaths::GetCleanFilename(InPakPath), PakFileSumary.MountPoint, true);

	UE_LOG(LogPakAnalyzer, Log, TEXT("Load all file info from pak."));

	// Iterate Files
	TArray<RecordIterator> Records;
	for (RecordIterator It(*PakFilePtr, true); It; ++It)
	{
		Records.Add(It);
	}

	struct FOffsetSort
	{
		FORCEINLINE bool operator()(const RecordIterator& A, const RecordIterator& B) const
		{
			return A.Info().Offset < B.Info().Offset;
		}
	};
	Records.Sort(FOffsetSort());

	{
		FScopeLock Lock(&CriticalSection);
		FPakEntry PakEntry;

		for (auto It : Records)
		{
			FPakTreeEntryPtr Child = nullptr;

			PakEntry = It.Info();

#if ENGINE_MINOR_VERSION >= 26
			PakFilePtr->ReadHashFromPayload(PakEntry, PakEntry.Hash);

			const FString* Filename = It.TryGetFilename();
			if (Filename)
			{
				Child = InsertFileToTree(*Filename, PakEntry);
			}
#else
			Child = InsertFileToTree(It.Filename(), PakEntry);
#endif
			if (Child.IsValid() && Child->Filename.ToString().EndsWith(TEXT("AssetRegistry.bin")))
			{
				LoadAssetRegistryFromPak(PakFilePtr, Child);
			}
		}

		RefreshTreeNode(TreeRoot);
		RefreshTreeNodeSizePercent(TreeRoot);
		RefreshClassMap(TreeRoot);
	}

	bHasPakLoaded = true;

	UE_LOG(LogPakAnalyzer, Log, TEXT("Finish load pak file: %s."), *InPakPath);

	return true;
}

int32 FPakAnalyzer::GetFileCount() const
{
	return TreeRoot.IsValid() ? TreeRoot->FileCount : 0;
}

void FPakAnalyzer::GetFiles(const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, TArray<FPakFileEntryPtr>& OutFiles) const
{
	FScopeLock Lock(const_cast<FCriticalSection*>(&CriticalSection));

	if (TreeRoot.IsValid())
	{
		RetriveFiles(TreeRoot, InFilterText, InClassFilterMap, OutFiles);
	}
}

FString FPakAnalyzer::GetLastLoadGuid() const
{
	return LoadGuid.ToString();
}

bool FPakAnalyzer::IsLoadDirty(const FString& InGuid) const
{
	return !InGuid.Equals(LoadGuid.ToString(), ESearchCase::IgnoreCase);
}

const FPakFileSumary& FPakAnalyzer::GetPakFileSumary() const
{
	return PakFileSumary;
}

FPakTreeEntryPtr FPakAnalyzer::GetPakTreeRootNode() const
{
	return TreeRoot;
}

FString FPakAnalyzer::ResolveCompressionMethod(const FPakEntry* InPakEntry) const
{
#if ENGINE_MINOR_VERSION >= 22
	if (InPakEntry->CompressionMethodIndex >= 0 && InPakEntry->CompressionMethodIndex < (uint32)PakFileSumary.PakInfo.CompressionMethods.Num())
	{
		return PakFileSumary.PakInfo.CompressionMethods[InPakEntry->CompressionMethodIndex].ToString();
	}
	else
	{
		return TEXT("Unknown");
	}
#else
	static const TArray<FString> CompressionMethods({ TEXT("None"), TEXT("Zlib"), TEXT("Gzip"), TEXT("Unknown"), TEXT("Custom") });

	if (InPakEntry->CompressionMethod >= 0 && InPakEntry->CompressionMethod < CompressionMethods.Num())
	{
		return CompressionMethods[InPakEntry->CompressionMethod];
	}
	else
	{
		return TEXT("Unknown");
	}
#endif
}

void FPakAnalyzer::ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles)
{
	const int32 WorkerCount = ExtractWorkers.Num();
	const int32 FileCount = InFiles.Num();

	if (FileCount <= 0 || WorkerCount <= 0)
	{
		return;
	}

	FPakAnalyzerDelegates::OnExtractStart.ExecuteIfBound();

	if (!FPaths::DirectoryExists(InOutputPath))
	{
		IFileManager::Get().MakeDirectory(*InOutputPath, true);
	}

	ShutdownAllExtractWorker();

	// Sort by original size
	InFiles.Sort([](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->PakEntry.UncompressedSize > B->PakEntry.UncompressedSize;
		});

	// dispatch files to workers
	TArray<TArray<FPakFileEntry>> TaskFiles;
	TaskFiles.AddZeroed(WorkerCount);

	TArray<int64> WorkerSize;
	WorkerSize.AddZeroed(WorkerCount);

	for (int32 i = 0; i < FileCount; ++i)
	{
		int32 MinIndex = 0;
		FMath::Min(WorkerSize, &MinIndex);

		TaskFiles[MinIndex].Add(*InFiles[i]);
		WorkerSize[MinIndex] += InFiles[i]->PakEntry.UncompressedSize;
	}

	ResetProgress();

	for (int32 i = 0; i < WorkerCount; ++i)
	{
		ExtractWorkers[i]->InitTaskFiles(TaskFiles[i]);
		ExtractWorkers[i]->StartExtract(PakFileSumary.PakFilePath, PakFileSumary.PakInfo.Version, CachedAESKey, InOutputPath);
	}
}

void FPakAnalyzer::CancelExtract()
{
	ShutdownAllExtractWorker();
}

bool FPakAnalyzer::ExportToJson(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles)
{
	UE_LOG(LogPakAnalyzer, Log, TEXT("Export to json: %s."), *InOutputPath);

	TSharedRef<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	RootObject->SetStringField(TEXT("Name"), FPaths::GetCleanFilename(PakFileSumary.PakFilePath));
	RootObject->SetStringField(TEXT("Path"), PakFileSumary.PakFilePath);
	RootObject->SetNumberField(TEXT("Pak File Size"), PakFileSumary.PakFileSize);
	RootObject->SetNumberField(TEXT("File Count"), TreeRoot->FileCount);
	RootObject->SetNumberField(TEXT("Total Size"), TreeRoot->Size);
	RootObject->SetNumberField(TEXT("Total Compressed Size"), TreeRoot->CompressedSize);
	RootObject->SetStringField(TEXT("MountPoint"), PakFileSumary.MountPoint);
	//RootObject->SetNumberField(TEXT("IndexSize"), PakFileSumary.PakInfo.IndexSize);
	//RootObject->SetNumberField(TEXT("IndexOffset"), PakFileSumary.PakInfo.IndexOffset);
	//RootObject->SetNumberField(TEXT("IndexEncrypted"), PakFileSumary.PakInfo.bEncryptedIndex);
	//RootObject->SetNumberField(TEXT("HeaderSize"), PakFileSumary.PakFileSize - PakFileSumary.PakInfo.IndexSize - PakFileSumary.PakInfo.IndexOffset);
	RootObject->SetNumberField(TEXT("PakVersion"), PakFileSumary.PakInfo.Version);

	TArray<TSharedPtr<FJsonValue>> FileObjects;

	int64 TotalSize = 0;
	int64 TotalCompressedSize = 0;

	TMap<FName, FPakClassEntry> ExportedClassMap;

	for (const FPakFileEntryPtr It : InFiles)
	{
		const FPakEntry& PakEntry = It->PakEntry;

		TSharedRef<FJsonObject> FileObject = MakeShareable(new FJsonObject);

		FileObject->SetStringField(TEXT("Name"), It->Filename.ToString());
		FileObject->SetStringField(TEXT("Path"), It->Path);
		FileObject->SetNumberField(TEXT("Offset"), PakEntry.Offset);
		FileObject->SetNumberField(TEXT("Size"), PakEntry.UncompressedSize);
		FileObject->SetNumberField(TEXT("Compressed Size"), PakEntry.Size);
		FileObject->SetNumberField(TEXT("Compressed Block Count"), PakEntry.CompressionBlocks.Num());
		FileObject->SetNumberField(TEXT("Compressed Block Size"), PakEntry.CompressionBlockSize);
		FileObject->SetStringField(TEXT("SHA1"), BytesToHex(PakEntry.Hash, sizeof(PakEntry.Hash)));
		FileObject->SetStringField(TEXT("IsEncrypted"), PakEntry.IsEncrypted() ? TEXT("True") : TEXT("False"));
		FileObject->SetStringField(TEXT("Class"), It->Class.ToString());

		FileObjects.Add(MakeShareable(new FJsonValueObject(FileObject)));

		TotalSize += PakEntry.UncompressedSize;
		TotalCompressedSize += PakEntry.Size;

		FPakClassEntry* ClassEntry = ExportedClassMap.Find(It->Class);
		if (ClassEntry)
		{
			ClassEntry->FileCount += 1;
			ClassEntry->CompressedSize += PakEntry.Size;
			ClassEntry->Size += PakEntry.UncompressedSize;
		}
		else
		{
			ExportedClassMap.Add(It->Class, FPakClassEntry(It->Class, PakEntry.UncompressedSize, PakEntry.Size, 1));
		}
	}

	RootObject->SetNumberField(TEXT("Exported File Count"), InFiles.Num());
	RootObject->SetNumberField(TEXT("Exported Total Size"), TotalSize);
	RootObject->SetNumberField(TEXT("Exported Total Compressed Size"), TotalCompressedSize);

	ExportedClassMap.ValueSort(
		[](const FPakClassEntry& A, const FPakClassEntry& B) -> bool
		{
			return A.CompressedSize > B.CompressedSize;
		});

	TArray<TSharedPtr<FJsonValue>> ClassObjects;
	for (const auto& Pair : ExportedClassMap)
	{
		const FPakClassEntry& ClassEntry = Pair.Value;

		TSharedRef<FJsonObject> ClassObject = MakeShareable(new FJsonObject);
		ClassObject->SetStringField(TEXT("Class"), ClassEntry.Class.ToString());
		ClassObject->SetNumberField(TEXT("File Count"), ClassEntry.FileCount);
		ClassObject->SetNumberField(TEXT("Size"), ClassEntry.Size);
		ClassObject->SetNumberField(TEXT("Compressed Size"), ClassEntry.CompressedSize);
		ClassObject->SetNumberField(TEXT("Compressed Size Percent Of Exported"), TotalCompressedSize > 0 ? 100 * (float)ClassEntry.CompressedSize / TotalCompressedSize : 0.f);

		ClassObjects.Add(MakeShareable(new FJsonValueObject(ClassObject)));
	}

	RootObject->SetArrayField(TEXT("Group By Class"), ClassObjects);
	RootObject->SetArrayField(TEXT("Files"), FileObjects);

	bool bExportResult = false;

	FString FileContents;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&FileContents);
	if (FJsonSerializer::Serialize(RootObject, JsonWriter))
	{
		JsonWriter->Close();
		bExportResult = FFileHelper::SaveStringToFile(FileContents, *InOutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	UE_LOG(LogPakAnalyzer, Log, TEXT("Export to json: %s finished, file count: %d, result: %d."), *InOutputPath, InFiles.Num(), bExportResult);

	return bExportResult;
}

bool FPakAnalyzer::ExportToCsv(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles)
{
	UE_LOG(LogPakAnalyzer, Log, TEXT("Export to csv: %s."), *InOutputPath);

	TArray<FString> Lines;
	Lines.Empty(InFiles.Num() + 2);
	Lines.Add(TEXT("Id, Name, Path, Offset, Class, Size, Compressed Size, Compressed Block Count, Compressed Block Size, SHA1, IsEncrypted"));

	int32 Index = 1;
	for (const FPakFileEntryPtr It : InFiles)
	{
		const FPakEntry& PakEntry = It->PakEntry;

		Lines.Add(FString::Printf(TEXT("%d, %s, %s, %lld, %s, %lld, %lld, %d, %d, %s, %s"),
			Index,
			*It->Filename.ToString(),
			*It->Path,
			PakEntry.Offset,
			*It->Class.ToString(),
			PakEntry.UncompressedSize,
			PakEntry.Size,
			PakEntry.CompressionBlocks.Num(),
			PakEntry.CompressionBlockSize,
			*BytesToHex(PakEntry.Hash, sizeof(PakEntry.Hash)),
			PakEntry.IsEncrypted() ? TEXT("True") : TEXT("False")));
		++Index;
	}

	const bool bExportResult = FFileHelper::SaveStringArrayToFile(Lines, *InOutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	UE_LOG(LogPakAnalyzer, Log, TEXT("Export to csv: %s finished, file count: %d, result: %d."), *InOutputPath, InFiles.Num(), bExportResult);

	return bExportResult;
}

bool FPakAnalyzer::HasPakLoaded() const
{
	return bHasPakLoaded;
}

void FPakAnalyzer::SetExtractThreadCount(int32 InThreadCount)
{
	const int32 ClampThreadCount = FMath::Clamp(InThreadCount, 1, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	if (ClampThreadCount != ExtractWorkerCount)
	{
		UE_LOG(LogPakAnalyzer, Log, TEXT("Set extract worker count: %d."), ClampThreadCount);

		ExtractWorkerCount = ClampThreadCount;
		InitializeExtractWorker();
	}
}

void FPakAnalyzer::Reset()
{
	LoadGuid.Invalidate();

	PakFileSumary.MountPoint = TEXT("");
	PakFileSumary.PakInfo = FPakInfo();
	PakFileSumary.PakFilePath = TEXT("");
	PakFileSumary.PakFileSize = 0;
	PakFileSumary.CompressionMethods = TEXT("");
	PakFileSumary.AssetRegistryPath = TEXT("");

	TreeRoot.Reset();
	AssetRegistryState.Reset();

	bHasPakLoaded = false;
}

FPakTreeEntryPtr FPakAnalyzer::InsertFileToTree(const FString& InFullPath, const FPakEntry& InPakEntry)
{
	static const TCHAR* Delims[2] = { TEXT("\\"), TEXT("/") };

	TArray<FString> PathItems;
	InFullPath.ParseIntoArray(PathItems, Delims, 2);

	if (PathItems.Num() <= 0)
	{
		return nullptr;
	}

	FString CurrentPath;
	FPakTreeEntryPtr Parent = TreeRoot;

	for (int32 i = 0; i < PathItems.Num(); ++i)
	{
		CurrentPath = CurrentPath / PathItems[i];

		FPakTreeEntryPtr* Child = Parent->ChildrenMap.Find(*PathItems[i]);
		if (Child)
		{
			Parent = *Child;
			continue;
		}
		else
		{
			const bool bLastItem = (i == PathItems.Num() - 1);

			FPakTreeEntryPtr NewChild = MakeShared<FPakTreeEntry>(PathItems[i], CurrentPath, !bLastItem);
			if (bLastItem)
			{
				NewChild->PakEntry = InPakEntry;
				NewChild->CompressionMethod = *ResolveCompressionMethod(&InPakEntry);
			}

			Parent->ChildrenMap.Add(*PathItems[i], NewChild);
			Parent = NewChild;
		}
	}

	return Parent;
}

void FPakAnalyzer::RefreshTreeNode(FPakTreeEntryPtr InRoot)
{
	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;
		if (Child->bIsDirectory)
		{
			RefreshTreeNode(Child);
		}
		else
		{
			Child->FileCount = 1;
			Child->Size = Child->PakEntry.UncompressedSize;
			Child->CompressedSize = Child->PakEntry.Size;
		}

		InRoot->FileCount += Child->FileCount;
		InRoot->Size += Child->Size;
		InRoot->CompressedSize += Child->CompressedSize;
	}

	InRoot->ChildrenMap.ValueSort([](const FPakTreeEntryPtr& A, const FPakTreeEntryPtr& B) -> bool
		{
			if (A->bIsDirectory == B->bIsDirectory)
			{
				return A->Filename.LexicalLess(B->Filename);
			}

			return (int32)A->bIsDirectory > (int32)B->bIsDirectory;
		});
}

void FPakAnalyzer::RefreshTreeNodeSizePercent(FPakTreeEntryPtr InRoot)
{
	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;
		Child->CompressedSizePercentOfTotal = TreeRoot->CompressedSize > 0 ? (float)Child->CompressedSize / TreeRoot->CompressedSize : 0.f;
		Child->CompressedSizePercentOfParent = InRoot->CompressedSize > 0 ? (float)Child->CompressedSize / InRoot->CompressedSize : 0.f;

		if (Child->bIsDirectory)
		{
			RefreshTreeNodeSizePercent(Child);
		}
	}
}

void FPakAnalyzer::RetriveFiles(FPakTreeEntryPtr InRoot, const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, TArray<FPakFileEntryPtr>& OutFiles) const
{
	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;
		if (Child->bIsDirectory)
		{
			RetriveFiles(Child, InFilterText, InClassFilterMap, OutFiles);
		}
		else
		{
			const bool* bShow = InClassFilterMap.Find(Child->Class);
			const bool bMatchClass = (InClassFilterMap.Num() <= 0 || (bShow && *bShow));

			if (bMatchClass && (InFilterText.IsEmpty() || /*Child->Filename.Contains(InFilterText) ||*/ Child->Path.Contains(InFilterText)))
			{
				FPakFileEntryPtr FileEntryPtr = MakeShared<FPakFileEntry>(Child->Filename.ToString(), Child->Path);
				if (!Child->bIsDirectory)
				{
					FileEntryPtr->Class = Child->Class;
					FileEntryPtr->PakEntry = Child->PakEntry;
					FileEntryPtr->CompressionMethod = Child->CompressionMethod;
				}

				OutFiles.Add(FileEntryPtr);
			}
		}
	}
}

void FPakAnalyzer::RefreshClassMap(FPakTreeEntryPtr InRoot)
{
	InRoot->FileClassMap.Empty();

	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;

		if (Child->bIsDirectory)
		{
			RefreshClassMap(Child);
			for (auto& ClassPair : Child->FileClassMap)
			{
				InsertClassInfo(InRoot, ClassPair.Key, ClassPair.Value->FileCount, ClassPair.Value->Size, ClassPair.Value->CompressedSize);
			}
		}
		else
		{
			Child->Class = GetAssetClass(Child->Path);
			InsertClassInfo(InRoot, Child->Class, 1, Child->Size, Child->CompressedSize);

		}
	}
}

void FPakAnalyzer::InsertClassInfo(FPakTreeEntryPtr InRoot, FName InClassName, int32 InFileCount, int64 InSize, int64 InCompressedSize)
{
	FPakClassEntryPtr* ClassEntryPtr = InRoot->FileClassMap.Find(InClassName);
	FPakClassEntryPtr ClassEntry = nullptr;

	if (!ClassEntryPtr)
	{
		ClassEntry = MakeShared<FPakClassEntry>(InClassName, InSize, InCompressedSize, InFileCount);
		InRoot->FileClassMap.Add(InClassName, ClassEntry);
	}
	else
	{
		ClassEntry = *ClassEntryPtr;
		ClassEntry->Class = InClassName;
		ClassEntry->FileCount += InFileCount;
		ClassEntry->Size += InSize;
		ClassEntry->CompressedSize += InCompressedSize;
	}

	ClassEntry->PercentOfTotal = TreeRoot->CompressedSize > 0 ? (float)ClassEntry->CompressedSize / TreeRoot->CompressedSize : 0.f;
	ClassEntry->PercentOfParent = InRoot->CompressedSize > 0 ? (float)ClassEntry->CompressedSize / InRoot->CompressedSize : 0.f;
}

FName FPakAnalyzer::GetAssetClass(const FString& InFilename)
{
	FName AssetClass = *FPaths::GetExtension(InFilename);
	if (AssetRegistryState.IsValid())
	{
		const FString FullPath = PakFileSumary.MountPoint / InFilename;
		FString Left, Right;
		if (FullPath.Split(TEXT("Content"), &Left, &Right))
		{
			const FString Prefix = FPaths::GetPathLeaf(Left);
			const bool bNotUseGamePrefix = Prefix == TEXT("Engine") || FullPath.Contains(TEXT("Plugin"));

			const FString FullObjectPath = FPaths::SetExtension(bNotUseGamePrefix ? TEXT("/") / Prefix / Right : TEXT("/Game") / Right, TEXT(""));

#if ENGINE_MINOR_VERSION >= 27
			TArrayView<FAssetData const* const> AssetDataArray = AssetRegistryState->GetAssetsByPackageName(*FullObjectPath);
#else
			const TArray<const FAssetData*>& AssetDataArray = AssetRegistryState->GetAssetsByPackageName(*FullObjectPath);
#endif
			if (AssetDataArray.Num() > 0)
			{
				AssetClass = AssetDataArray[0]->AssetClass;
			}
		}
	}

	return AssetClass.IsNone() ? TEXT("Unknown") : AssetClass;
}

bool FPakAnalyzer::LoadAssetRegistryFromPak(FPakFile* InPakFile, FPakFileEntryPtr InPakFileEntry)
{
	if (!InPakFile || !InPakFile->IsValid() || !InPakFileEntry.IsValid())
	{
		return false;
	}

	const bool bHasRelativeCompressedChunkOffsets = InPakFile->GetInfo().Version >= FPakInfo::PakFile_Version_RelativeChunkOffsets;

	const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
	void* Buffer = FMemory::Malloc(BufferSize);
	uint8* PersistantCompressionBuffer = NULL;
	int64 CompressionBufferSize = 0;

	bool bReadResult = true;
	const FPakEntry& EntryInfo = InPakFileEntry->PakEntry;

	FArrayReader ContentReader;
	ContentReader.AddZeroed(InPakFileEntry->PakEntry.UncompressedSize);

	FMemoryWriter ContentWriter(ContentReader);

	if (EntryInfo.CompressionMethodIndex == 0)
	{
		if (!FExtractThreadWorker::BufferedCopyFile(ContentWriter, *InPakFile->GetSharedReader(nullptr), EntryInfo, Buffer, BufferSize, CachedAESKey))
		{
			bReadResult = false;
		}
	}
	else
	{
		if (!FExtractThreadWorker::UncompressCopyFile(ContentWriter, *InPakFile->GetSharedReader(nullptr), EntryInfo, PersistantCompressionBuffer, CompressionBufferSize, CachedAESKey, InPakFileEntry->CompressionMethod, bHasRelativeCompressedChunkOffsets))
		{
			bReadResult = false;
		}
	}

	FMemory::Free(Buffer);
	FMemory::Free(PersistantCompressionBuffer);

	if (!bReadResult)
	{
		return false;
	}

	const bool bLoadResult = LoadAssetRegistry(ContentReader);
	if (bLoadResult)
	{
		PakFileSumary.AssetRegistryPath = InPakFileEntry->Path;
	}

	return bLoadResult;
}

bool FPakAnalyzer::LoadAssetRegistry(FArrayReader& InData)
{
	FAssetRegistrySerializationOptions LoadOptions;
	LoadOptions.bSerializeDependencies = false;
	LoadOptions.bSerializePackageData = false;

	TSharedPtr<FAssetRegistryState> NewAssetRegistryState = MakeShared<FAssetRegistryState>();
	if (NewAssetRegistryState->Serialize(InData, LoadOptions))
	{
		AssetRegistryState = NewAssetRegistryState;
		return true;
	}

	return false;
}

bool FPakAnalyzer::LoadAssetRegistry(const FString& InRegristryPath)
{
	if (!bHasPakLoaded)
	{
		return false;
	}

	FArrayReader ContentReader;
	if (!FFileHelper::LoadFileToArray(ContentReader, *InRegristryPath))
	{
		return false;
	}

	if (!LoadAssetRegistry(ContentReader))
	{
		return false;
	}

	PakFileSumary.AssetRegistryPath = FPaths::ConvertRelativePathToFull(InRegristryPath);

	RefreshClassMap(TreeRoot);
	return true;
}

bool FPakAnalyzer::PreLoadPak(const FString& InPakPath)
{
	UE_LOG(LogPakAnalyzer, Log, TEXT("Pre load pak file: %s and check file hash."), *InPakPath);

	FArchive* Reader = IFileManager::Get().CreateFileReader(*InPakPath);
	if (!Reader)
	{
		return false;
	}

	FPakInfo Info;
	const int64 CachedTotalSize = Reader->TotalSize();
	bool bShouldLoad = false;
	int32 CompatibleVersion = FPakInfo::PakFile_Version_Latest;

	// Serialize trailer and check if everything is as expected.
	// start up one to offset the -- below
	CompatibleVersion++;
	int64 FileInfoPos = -1;
	do
	{
		// try the next version down
		CompatibleVersion--;

		FileInfoPos = CachedTotalSize - Info.GetSerializedSize(CompatibleVersion);
		if (FileInfoPos >= 0)
		{
			Reader->Seek(FileInfoPos);

			// Serialize trailer and check if everything is as expected.
			Info.Serialize(*Reader, CompatibleVersion);
			if (Info.Magic == FPakInfo::PakFile_Magic)
			{
				bShouldLoad = true;
			}
		}
	} while (!bShouldLoad && CompatibleVersion >= FPakInfo::PakFile_Version_Initial);

	if (!bShouldLoad)
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("%s is not a valid pak file!"), *InPakPath);
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("%s is not a valid pak file!"), *InPakPath));

		Reader->Close();
		delete Reader;
		return false;
	}

	if (Info.EncryptionKeyGuid.IsValid() || Info.bEncryptedIndex)
	{
		const FString KeyString = FPakAnalyzerDelegates::OnGetAESKey.IsBound() ? FPakAnalyzerDelegates::OnGetAESKey.Execute() : TEXT("");

		FAES::FAESKey AESKey;

		TArray<uint8> DecodedBuffer;
		if (!FBase64::Decode(KeyString, DecodedBuffer))
		{
			UE_LOG(LogPakAnalyzer, Error, TEXT("AES encryption key base64[%s] is not base64 format!"), *KeyString);
			FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("AES encryption key[%s] is not base64 format!"), *KeyString));
			bShouldLoad = false;
		}

		// Error checking
		if (bShouldLoad && DecodedBuffer.Num() != FAES::FAESKey::KeySize)
		{
			UE_LOG(LogPakAnalyzer, Error, TEXT("AES encryption key base64[%s] can not decode to %d bytes long!"), *KeyString, FAES::FAESKey::KeySize);
			FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("AES encryption key base64[%s] can not decode to %d bytes long!"), *KeyString, FAES::FAESKey::KeySize));
			bShouldLoad = false;
		}

		if (bShouldLoad)
		{
			FMemory::Memcpy(AESKey.Key, DecodedBuffer.GetData(), FAES::FAESKey::KeySize);

			TArray<uint8> PrimaryIndexData;
			Reader->Seek(Info.IndexOffset);
			PrimaryIndexData.SetNum(Info.IndexSize);
			Reader->Serialize(PrimaryIndexData.GetData(), Info.IndexSize);

			if (!ValidateEncryptionKey(PrimaryIndexData, Info.IndexHash, AESKey))
			{
				UE_LOG(LogPakAnalyzer, Error, TEXT("AES encryption key base64[%s] is not correct!"), *KeyString);
				FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("AES encryption key base64[%s] is not correct!"), *KeyString));
				bShouldLoad = false;
			}
			else
			{
				CachedAESKey = AESKey;

				UE_LOG(LogPakAnalyzer, Log, TEXT("Use AES encryption key base64[%s] for %s."), *KeyString, *InPakPath);
				FCoreDelegates::GetPakEncryptionKeyDelegate().BindLambda(
					[AESKey](uint8 OutKey[32])
					{
						FMemory::Memcpy(OutKey, AESKey.Key, 32);
					});

				if (Info.EncryptionKeyGuid.IsValid())
				{
#if ENGINE_MINOR_VERSION >= 26
					FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().Broadcast(Info.EncryptionKeyGuid, AESKey);
#else
					FCoreDelegates::GetRegisterEncryptionKeyDelegate().ExecuteIfBound(Info.EncryptionKeyGuid, AESKey);
#endif
				}
			}
		}
	}

	Reader->Close();
	delete Reader;

	return bShouldLoad;
}

bool FPakAnalyzer::ValidateEncryptionKey(TArray<uint8>& IndexData, const FSHAHash& InExpectedHash, const FAES::FAESKey& InAESKey)
{
	FAES::DecryptData(IndexData.GetData(), IndexData.Num(), InAESKey);

	// Check SHA1 value.
	FSHAHash ActualHash;
	FSHA1::HashBuffer(IndexData.GetData(), IndexData.Num(), ActualHash.Hash);
	return InExpectedHash == ActualHash;
}

void FPakAnalyzer::InitializeExtractWorker()
{
	UE_LOG(LogPakAnalyzer, Log, TEXT("Initialize extract worker count: %d."), ExtractWorkerCount);

	ShutdownAllExtractWorker();

	ExtractWorkers.Empty();
	for (int32 i = 0; i < ExtractWorkerCount; ++i)
	{
		TSharedPtr<FExtractThreadWorker> Worker = MakeShared<FExtractThreadWorker>();
		Worker->GetOnUpdateExtractProgressDelegate().BindRaw(this, &FPakAnalyzer::OnUpdateExtractProgress);

		ExtractWorkers.Add(Worker);
	}

	ResetProgress();
}

void FPakAnalyzer::ShutdownAllExtractWorker()
{
	for (TSharedPtr<FExtractThreadWorker> Worker : ExtractWorkers)
	{
		Worker->Shutdown();
	}
}

void FPakAnalyzer::OnUpdateExtractProgress(const FGuid& WorkerGuid, int32 CompleteCount, int32 ErrorCount, int32 TotalCount)
{
	FFunctionGraphTask::CreateAndDispatchWhenReady([this, &WorkerGuid, CompleteCount, ErrorCount, TotalCount]()
		{
			FExtractProgress& Progress = ExtractWorkerProgresses.FindOrAdd(WorkerGuid);

			Progress.CompleteCount = CompleteCount;
			Progress.ErrorCount = ErrorCount;
			Progress.TotalCount = TotalCount;

			int32 TotalCompleteCount = 0;
			int32 TotalErrorCount = 0;
			int32 TotalTotalCount = 0;
			for (const auto& It : ExtractWorkerProgresses)
			{
				TotalCompleteCount += It.Value.CompleteCount;
				TotalErrorCount += It.Value.ErrorCount;
				TotalTotalCount += It.Value.TotalCount;
			}

			FPakAnalyzerDelegates::OnUpdateExtractProgress.ExecuteIfBound(TotalCompleteCount, TotalErrorCount, TotalTotalCount);
		},
		TStatId(), nullptr, ENamedThreads::GameThread);
}

void FPakAnalyzer::ResetProgress()
{
	ExtractWorkerProgresses.Empty(ExtractWorkers.Num());
}
