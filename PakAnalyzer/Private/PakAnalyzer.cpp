// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PakAnalyzer.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformMisc.h"
#include "Json.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
// #include "Serialization/Archive.h"
// #include "Serialization/MemoryWriter.h"

#include "AssetParseThreadWorker.h"
#include "CommonDefines.h"
#include "ExtractThreadWorker.h"

typedef FPakFile::FPakEntryIterator RecordIterator;

FPakAnalyzer::FPakAnalyzer()
	: ExtractWorkerCount(DEFAULT_EXTRACT_THREAD_COUNT)
{
	Reset();
	InitializeExtractWorker();
	InitializeAssetParseWorker();
}

FPakAnalyzer::~FPakAnalyzer()
{
	ShutdownAssetParseWorker();
	ShutdownAllExtractWorker();
	Reset();
}

FPakTreeEntryPtr FPakAnalyzer::LoadPakFile(const FString& InPakPath, const FString& InDefaultAESKey/* = TEXT("")*/)
{
	if (InPakPath.IsEmpty())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Pak path is empty!"));
		return nullptr;
	}

	UE_LOG(LogPakAnalyzer, Log, TEXT("Start load pak file: %s."), *InPakPath);

	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	if (!PlatformFile.FileExists(*InPakPath))
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Load pak file failed! Pak file not exists! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Pak file not exists! Path: %s."), *InPakPath);
		return nullptr;
	}

	FString DecryptAESKey;
	if (!PreLoadPak(InPakPath, InDefaultAESKey, DecryptAESKey))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Pre load pak file failed! Path: %s."), *InPakPath);
		return nullptr;
	}
	
	TRefCountPtr<FPakFile> PakFile = new FPakFile(*InPakPath, false);
	FPakFile* PakFilePtr = PakFile.GetReference();
	if (!PakFilePtr)
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Load pak file failed! Create PakFile failed! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Create PakFile failed! Path: %s."), *InPakPath);

		return nullptr;
	}

	if (!PakFilePtr->IsValid())
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Load pak file failed! Unable to open pak file! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Unable to open pak file! Path: %s."), *InPakPath);

		return nullptr;
	}

	// Save pak sumary
	FPakFileSumaryPtr Summary = MakeShared<FPakFileSumary>();
	PakFileSummaries.Add(Summary);
	const int32 SummaryIndex = PakFileSummaries.Num() - 1;

	Summary->MountPoint = PakFilePtr->GetMountPoint();
	Summary->PakInfo = PakFilePtr->GetInfo();
	Summary->PakFilePath = InPakPath;
	Summary->PakFileSize = PakFilePtr->TotalSize();
	Summary->DecryptAESKeyStr = DecryptAESKey;
	if (!FBase64::Decode(*DecryptAESKey, DecryptAESKey.Len(), Summary->DecryptAESKey.Key))
	{
		Summary->DecryptAESKey.Reset();
	}

	TArray<FString> Methods;
	for (const FName& Name : Summary->PakInfo.CompressionMethods)
	{
		Methods.Add(Name.ToString());
	}
	Summary->CompressionMethods = FString::Join(Methods, TEXT(", "));

	// Make tree root
	FPakTreeEntryPtr PakTreeRoot = MakeShared<FPakTreeEntry>(*FPaths::GetCleanFilename(InPakPath), Summary->MountPoint, true);

	UE_LOG(LogPakAnalyzer, Log, TEXT("Load all file info from pak."));

	// Iterate Files
	struct FPakEntryWithFilename
	{
		FPakEntry Entry;
		FString Filename;
	};

	TArray<FPakEntryWithFilename> Records;
	for (RecordIterator It(*PakFilePtr, true); It; ++It)
	{
		const FString& Filename = *It.TryGetFilename();
		Records.Add({ It.Info(), Filename });
	}

	{
		FScopeLock Lock(&CriticalSection);

		for (FPakEntryWithFilename& Record : Records)
		{
			FPakTreeEntryPtr Child = nullptr;

			FPakEntry& PakEntry = Record.Entry;

			if (PakEntry.CompressionBlocks.Num() == 1)
			{
				PakEntry.CompressionBlockSize = PakEntry.UncompressedSize;
			}
			
			PakFilePtr->ReadHashFromPayload(PakEntry, PakEntry.Hash);

			FString FullFilePath = Summary->MountPoint / Record.Filename;
			FullFilePath.ReplaceInline(TEXT("../"), TEXT(""));
			FullFilePath.ReplaceInline(TEXT("..\\"), TEXT(""));

			Child = InsertFileToTree(PakTreeRoot, *Summary, FullFilePath, PakEntry);
			Child->OwnerPakIndex = SummaryIndex;
			if (Child.IsValid() && Child->Filename.ToString().EndsWith(TEXT("AssetRegistry.bin")))
			{
				LoadAssetRegistryFromPak(PakFilePtr, Child, Summary->DecryptAESKey);
			}
		}
	}

	RefreshTreeNode(PakTreeRoot);
	RefreshTreeNodeSizePercent(PakTreeRoot, PakTreeRoot);

	Summary->FileCount = PakTreeRoot->FileCount;

	UE_LOG(LogPakAnalyzer, Log, TEXT("Finish load pak file: %s."), *InPakPath);

	return PakTreeRoot;
}

bool FPakAnalyzer::LoadPakFiles(const TArray<FString>& InPakPaths, const TArray<FString>& InDefaultAESKeys, int32 ContainerStartIndex)
{
	TArray<FString> PakFiles;
	TArray<FString> UsedDefaultAESKeys;
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	
	for (int32 i = 0; i < InPakPaths.Num(); ++i)
	{
		const FString& PakPath = InPakPaths[i];
		if (PlatformFile.FileExists(*PakPath) && PakPath.EndsWith(".pak"))
		{
			PakFiles.Add(PakPath);
			UsedDefaultAESKeys.Add(InDefaultAESKeys.IsValidIndex(i) ? InDefaultAESKeys[i] : TEXT(""));
		}
	}

	if (PakFiles.Num() <= 0)
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(TEXT("Load pak file failed! Pak files not exists!"));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Pak files not exists!"));
		return false;
	}

	Reset();
	DefaultAESKeys = UsedDefaultAESKeys;

	for (int32 i = 0; i < PakFiles.Num(); ++i)
	{
		FPakTreeEntryPtr PakTreeRoot = LoadPakFile(PakFiles[i], DefaultAESKeys.IsValidIndex(i) ? DefaultAESKeys[i] : TEXT(""));
		if (PakTreeRoot.IsValid())
		{
			PakTreeRoots.Add(PakTreeRoot);
		}
	}

	if (!AssetRegistryPath.IsEmpty())
	{
		for (const FPakTreeEntryPtr& PakTreeRoot : PakTreeRoots)
		{
			RefreshClassMap(PakTreeRoot, PakTreeRoot);
			RefreshPackageDependency(PakTreeRoot, PakTreeRoot);
		}
	}

	ParseAssetFile();

	//FPakAnalyzerDelegates::OnPakLoadFinish.Broadcast();

	return true;
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

	TArray<FPakFileSumary> Summaries;
	Summaries.AddDefaulted(PakFileSummaries.Num());
	for (int32 i = 0; i < PakFileSummaries.Num(); ++i)
	{
		Summaries[i] = *PakFileSummaries[i];
	}

	for (int32 i = 0; i < WorkerCount; ++i)
	{
		ExtractWorkers[i]->InitTaskFiles(TaskFiles[i]);
		ExtractWorkers[i]->StartExtract(Summaries, InOutputPath);
	}
}

void FPakAnalyzer::CancelExtract()
{
	ShutdownAllExtractWorker();
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
	ShutdownAssetParseWorker();
	DefaultAESKeys.Empty();

	FBaseAnalyzer::Reset();
}

bool FPakAnalyzer::LoadAssetRegistryFromPak(FPakFile* InPakFile, FPakFileEntryPtr InPakFileEntry, const FAES::FAESKey& DecryptAESKey)
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
		if (!FExtractThreadWorker::BufferedCopyFile(ContentWriter, InPakFile->GetSharedReader(nullptr).GetArchive(), EntryInfo, Buffer, BufferSize, DecryptAESKey))
		{
			bReadResult = false;
		}
	}
	else
	{
		if (!FExtractThreadWorker::UncompressCopyFile(ContentWriter, InPakFile->GetSharedReader(nullptr).GetArchive(), EntryInfo, PersistantCompressionBuffer, CompressionBufferSize, DecryptAESKey, InPakFileEntry->CompressionMethod, bHasRelativeCompressedChunkOffsets))
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
		AssetRegistryPath = InPakFileEntry->Path;
	}
	
	return bLoadResult;
}

bool FPakAnalyzer::PreLoadPak(const FString& InPakPath, const FString& InDefaultAESKey, FString& OutDecryptKey)
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
		OutDecryptKey = InDefaultAESKey;
		bShouldLoad = InDefaultAESKey.IsEmpty() ? false : TryDecryptPak(Reader, Info, InDefaultAESKey, false);

		if (!bShouldLoad)
		{
			if (FPakAnalyzerDelegates::OnGetAESKey.IsBound())
			{
				bool bCancel = true;
				do
				{
					OutDecryptKey = FPakAnalyzerDelegates::OnGetAESKey.Execute(InPakPath, Info.EncryptionKeyGuid, bCancel);

					bShouldLoad = !bCancel ? TryDecryptPak(Reader, Info, OutDecryptKey, true) : false;
				} while (!bShouldLoad && !bCancel);
			}
			else
			{
				UE_LOG(LogPakAnalyzer, Error, TEXT("Can't open encrypt pak without OnGetAESKey bound!"));
				FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Can't open encrypt pak without OnGetAESKey bound!")));
				bShouldLoad = false;
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

bool FPakAnalyzer::TryDecryptPak(FArchive* InReader, const FPakInfo& InPakInfo, const FString& InKey, bool bShowWarning)
{
	const FString KeyString = InKey;
	bool bShouldLoad = true;

	FAES::FAESKey AESKey;

	TArray<uint8> DecodedBuffer;
	if (!FBase64::Decode(KeyString, DecodedBuffer))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("AES encryption key base64[%s] is not base64 format!"), *KeyString);

		if (bShowWarning)
		{
			FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("AES encryption key[%s] is not base64 format!"), *KeyString));
		}
		
		bShouldLoad = false;
	}

	// Error checking
	if (bShouldLoad && DecodedBuffer.Num() != FAES::FAESKey::KeySize)
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("AES encryption key base64[%s] can not decode to %d bytes long!"), *KeyString, FAES::FAESKey::KeySize);

		if (bShowWarning)
		{
			FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("AES encryption key base64[%s] can not decode to %d bytes long!"), *KeyString, FAES::FAESKey::KeySize));
		}
		
		bShouldLoad = false;
	}

	if (bShouldLoad)
	{
		FMemory::Memcpy(AESKey.Key, DecodedBuffer.GetData(), FAES::FAESKey::KeySize);

		TArray<uint8> PrimaryIndexData;
		InReader->Seek(InPakInfo.IndexOffset);
		PrimaryIndexData.SetNum(InPakInfo.IndexSize);
		InReader->Serialize(PrimaryIndexData.GetData(), InPakInfo.IndexSize);

		if (!ValidateEncryptionKey(PrimaryIndexData, InPakInfo.IndexHash, AESKey))
		{
			UE_LOG(LogPakAnalyzer, Error, TEXT("AES encryption key base64[%s] is not correct!"), *KeyString);

			if (bShowWarning)
			{
				FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("AES encryption key base64[%s] is not correct!"), *KeyString));
			}

			bShouldLoad = false;
		}
		else
		{
			UE_LOG(LogPakAnalyzer, Log, TEXT("Use AES encryption key base64[%s]."), *KeyString);
			FCoreDelegates::GetPakEncryptionKeyDelegate().BindLambda(
				[AESKey](uint8 OutKey[32])
				{
					FMemory::Memcpy(OutKey, AESKey.Key, 32);
				});

			if (InPakInfo.EncryptionKeyGuid.IsValid())
			{
				FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().Broadcast(InPakInfo.EncryptionKeyGuid, AESKey);
			}
		}
	}

	return bShouldLoad;
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

void FPakAnalyzer::ParseAssetFile()
{
	if (AssetParseWorker.IsValid())
	{
		TArray<FPakFileEntryPtr> UAssetFiles;

		for (const FPakTreeEntryPtr& PakTreeRoot : PakTreeRoots)
		{
			RetriveUAssetFiles(PakTreeRoot, UAssetFiles);
		}

		if (UAssetFiles.Num() > 0)
		{
			TArray<FPakFileSumary> Summaries;
			Summaries.AddDefaulted(PakFileSummaries.Num());
			for (int32 i = 0; i < PakFileSummaries.Num(); ++i)
			{
				Summaries[i] = *PakFileSummaries[i];
			}

			AssetParseWorker->StartParse(UAssetFiles, Summaries);
		}
	}
}

void FPakAnalyzer::InitializeAssetParseWorker()
{
	UE_LOG(LogPakAnalyzer, Log, TEXT("Initialize asset parse worker."));

	if (!AssetParseWorker.IsValid())
	{
		AssetParseWorker = MakeShared<FAssetParseThreadWorker>();
		AssetParseWorker->OnParseFinish.BindRaw(this, &FPakAnalyzer::OnAssetParseFinish);
	}
}

void FPakAnalyzer::ShutdownAssetParseWorker()
{
	if (AssetParseWorker.IsValid())
	{
		AssetParseWorker->Shutdown();
	}
}

void FPakAnalyzer::OnAssetParseFinish(bool bCancel, const TMap<FName, FName>& ClassMap)
{
	if (bCancel)
	{
		return;
	}

	DefaultClassMap = ClassMap;
	const bool bRefreshClass = ClassMap.Num() > 0;

	FFunctionGraphTask::CreateAndDispatchWhenReady([this, bRefreshClass]()
		{
			if (bRefreshClass)
			{
				for (const FPakTreeEntryPtr& PakTreeRoot : PakTreeRoots)
				{
					RefreshClassMap(PakTreeRoot, PakTreeRoot);
				}
			}

			FPakAnalyzerDelegates::OnAssetParseFinish.Broadcast();
		},
		TStatId(), nullptr, ENamedThreads::GameThread);
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
