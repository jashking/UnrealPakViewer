// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FolderAnalyzer.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformMisc.h"
#include "HAL/UnrealMemory.h"
#include "Json.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryWriter.h"

#include "AssetParseThreadWorker.h"
#include "CommonDefines.h"

FFolderAnalyzer::FFolderAnalyzer()
{
	Reset();
	InitializeAssetParseWorker();
}

FFolderAnalyzer::~FFolderAnalyzer()
{
	ShutdownAssetParseWorker();
	Reset();
}

bool FFolderAnalyzer::LoadPakFiles(const TArray<FString>& InPakPaths, const TArray<FString>& InDefaultAESKeys, int32 ContainerStartIndex)
{
	const FString InPakPath = InPakPaths.Num() > 0 ? InPakPaths[0] : TEXT("");
	if (InPakPath.IsEmpty())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Open folder failed! Folder path is empty!"));
		return false;
	}

	UE_LOG(LogPakAnalyzer, Log, TEXT("Start open folder: %s."), *InPakPath);

	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	if (!PlatformFile.DirectoryExists(*InPakPath))
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Open folder failed! Folder not exists! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Open folder failed! Folder not exists! Path: %s."), *InPakPath);
		return false;
	}

	Reset();

	FPakFileSumaryPtr Summary = MakeShared<FPakFileSumary>();
	PakFileSummaries.Add(Summary);

	// Save pak sumary
	Summary->MountPoint = InPakPath;
	FMemory::Memset(&Summary->PakInfo, 0, sizeof(Summary->PakInfo));
	Summary->PakFilePath = InPakPath;

	ShutdownAssetParseWorker();

	// Make tree root
	FPakTreeEntryPtr TreeRoot = MakeShared<FPakTreeEntry>(*FPaths::GetCleanFilename(InPakPath), Summary->MountPoint, true);

	TArray<FString> FoundFiles;
	PlatformFile.FindFilesRecursively(FoundFiles, *InPakPath, TEXT(""));

	int64 TotalSize = 0;
	for (const FString& File : FoundFiles)
	{
		FPakEntry Entry;
		Entry.Offset = 0;
		Entry.UncompressedSize = PlatformFile.FileSize(*File);
		Entry.Size = Entry.UncompressedSize;

		TotalSize += Entry.UncompressedSize;

		FString RelativeFilename = File;
		RelativeFilename.RemoveFromStart(InPakPath);

		InsertFileToTree(TreeRoot, *Summary, RelativeFilename, Entry);

		if (File.Contains(TEXT("DevelopmentAssetRegistry.bin")))
		{
			AssetRegistryPath = File;
		}
	}

	Summary->PakFileSize = TotalSize;
	Summary->FileCount = TreeRoot->FileCount;

	RefreshTreeNode(TreeRoot);
	RefreshTreeNodeSizePercent(TreeRoot, TreeRoot);

	PakTreeRoots.Add(TreeRoot);

	if (!AssetRegistryPath.IsEmpty())
	{
		LoadAssetRegistry(AssetRegistryPath);
	}

	ParseAssetFile(TreeRoot);

	UE_LOG(LogPakAnalyzer, Log, TEXT("Finish load pak file: %s."), *InPakPath);

	FPakAnalyzerDelegates::OnPakLoadFinish.Broadcast();

	return true;
}

void FFolderAnalyzer::ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles)
{
}

void FFolderAnalyzer::CancelExtract()
{
}

void FFolderAnalyzer::SetExtractThreadCount(int32 InThreadCount)
{
}

void FFolderAnalyzer::ParseAssetFile(FPakTreeEntryPtr InRoot)
{
	if (AssetParseWorker.IsValid())
	{
		TArray<FPakFileEntryPtr> UAssetFiles;
		RetriveUAssetFiles(InRoot, UAssetFiles);

		TArray<FPakFileSumary> Summaries = { *PakFileSummaries[0] };
		AssetParseWorker->StartParse(UAssetFiles, Summaries);
	}
}

void FFolderAnalyzer::InitializeAssetParseWorker()
{
	UE_LOG(LogPakAnalyzer, Log, TEXT("Initialize asset parse worker."));

	if (!AssetParseWorker.IsValid())
	{
		AssetParseWorker = MakeShared<FAssetParseThreadWorker>();
		AssetParseWorker->OnReadAssetContent.BindRaw(this, &FFolderAnalyzer::OnReadAssetContent);
		AssetParseWorker->OnParseFinish.BindRaw(this, &FFolderAnalyzer::OnAssetParseFinish);
	}
}

void FFolderAnalyzer::ShutdownAssetParseWorker()
{
	if (AssetParseWorker.IsValid())
	{
		AssetParseWorker->Shutdown();
	}
}

void FFolderAnalyzer::OnReadAssetContent(FPakFileEntryPtr InFile, bool& bOutSuccess, TArray<uint8>& OutContent)
{
	bOutSuccess = false;
	OutContent.Empty();

	if (!InFile.IsValid())
	{
		return;
	}

	const FString FilePath = PakFileSummaries[0]->MountPoint / InFile->Path;
	bOutSuccess = FFileHelper::LoadFileToArray(OutContent, *FilePath);
}

void FFolderAnalyzer::OnAssetParseFinish(bool bCancel, const TMap<FName, FName>& ClassMap)
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
