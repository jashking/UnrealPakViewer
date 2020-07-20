// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PakAnalyzer.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "Launch/Resources/Version.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/Archive.h"

#include "CommonDefines.h"
#include "ExtractThreadWorker.h"

#if ENGINE_MINOR_VERSION >= 26
typedef FPakFile::FPakEntryIterator RecordIterator;
#else
typedef FPakFile::FFileIterator RecordIterator;
#endif

FPakAnalyzer::FPakAnalyzer()
	: ExtractWorkerCount(4)
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

	TUniquePtr<FPakFile> PakFile = MakeUnique<FPakFile>(*InPakPath, false);
	if (!PakFile.IsValid())
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Load pak file failed! Create PakFile failed! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Create PakFile failed! Path: %s."), *InPakPath);

		return false;
	}

	if (!PakFile->IsValid())
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Load pak file failed! Unable to open pak file! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Unable to open pak file! Path: %s."), *InPakPath);

		return false;
	}

	// Generate unique id
	LoadGuid = FGuid::NewGuid();

	// Save pak sumary
	PakFileSumary.MountPoint = PakFile->GetMountPoint();
	PakFileSumary.PakInfo = PakFile->GetInfo();
	PakFileSumary.PakFilePath = InPakPath;
	PakFileSumary.PakFileSize = PakFile->TotalSize();

	// Make tree root
	TreeRoot = MakeShared<FPakTreeEntry>(FPaths::GetCleanFilename(InPakPath), PakFileSumary.MountPoint, true);

	UE_LOG(LogPakAnalyzer, Log, TEXT("Load all file info from pak."));

	// Iterate Files
	TArray<RecordIterator> Records;
	for (RecordIterator It(*PakFile, true); It; ++It)
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
			PakEntry = It.Info();

#if ENGINE_MINOR_VERSION >= 26
			PakFile->ReadHashFromPayload(PakEntry, PakEntry.Hash);

			const FString* Filename = It.TryGetFilename();
			if (Filename)
			{
				InsertFileToTree(*Filename, PakEntry);
			}
#else
			InsertFileToTree(It.Filename(), PakEntry);
#endif
		}

		RefreshTreeNode(TreeRoot);
		RefreshTreeNodeSizePercent(TreeRoot);
	}

	UE_LOG(LogPakAnalyzer, Log, TEXT("Finish load pak file: %s."), *InPakPath);

	return true;
}

int32 FPakAnalyzer::GetFileCount() const
{
	return TreeRoot.IsValid() ? TreeRoot->FileCount : 0;
}

void FPakAnalyzer::GetFiles(const FString& InFilterText, TArray<FPakFileEntryPtr>& OutFiles) const
{
	FScopeLock Lock(const_cast<FCriticalSection*>(&CriticalSection));

	RetriveFiles(TreeRoot, InFilterText, OutFiles);
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

void FPakAnalyzer::Reset()
{
	LoadGuid.Invalidate();

	PakFileSumary.MountPoint = TEXT("");
	PakFileSumary.PakInfo = FPakInfo();
	PakFileSumary.PakFilePath = TEXT("");
	PakFileSumary.PakFileSize = 0;

	TreeRoot.Reset();
}

void FPakAnalyzer::InsertFileToTree(const FString& InFullPath, const FPakEntry& InPakEntry)
{
	static const TCHAR* Delims[2] = { TEXT("\\"), TEXT("/") };

	TArray<FString> PathItems;
	InFullPath.ParseIntoArray(PathItems, Delims, 2);

	if (PathItems.Num() <= 0)
	{
		return;
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
		Child->CompressedSizePercentOfTotal = (float)Child->CompressedSize / TreeRoot->CompressedSize;
		Child->CompressedSizePercentOfParent = (float)Child->CompressedSize / InRoot->CompressedSize;

		if (Child->bIsDirectory)
		{
			RefreshTreeNodeSizePercent(Child);
		}
	}
}

void FPakAnalyzer::RetriveFiles(FPakTreeEntryPtr InRoot, const FString& InFilterText, TArray<FPakFileEntryPtr>& OutFiles) const
{
	for (auto& Pair : InRoot->ChildrenMap)
	{
		FPakTreeEntryPtr Child = Pair.Value;
		if (Child->bIsDirectory)
		{
			RetriveFiles(Child, InFilterText, OutFiles);
		}
		else
		{
			if (InFilterText.IsEmpty() || /*Child->Filename.Contains(InFilterText) ||*/ Child->Path.Contains(InFilterText))
			{
				FPakFileEntryPtr FileEntryPtr = MakeShared<FPakFileEntry>(Child->Filename.ToString(), Child->Path);
				if (!Child->bIsDirectory)
				{
					FileEntryPtr->PakEntry = Child->PakEntry;
					FileEntryPtr->CompressionMethod = Child->CompressionMethod;
				}

				OutFiles.Add(FileEntryPtr);
			}
		}
	}
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
