// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FolderAnalyzer.h"

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

bool FFolderAnalyzer::LoadPakFile(const FString& InPakPath, const FString& InAESKey)
{
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

	// Generate unique id
	LoadGuid = FGuid::NewGuid();

	// Save pak sumary
	PakFileSumary.MountPoint = InPakPath;
	//PakFileSumary.PakInfo = PakFilePtr->GetInfo();
	PakFileSumary.PakFilePath = InPakPath;

	ShutdownAssetParseWorker();

	// Make tree root
	TreeRoot = MakeShared<FPakTreeEntry>(*FPaths::GetCleanFilename(InPakPath), PakFileSumary.MountPoint, true);

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

		InsertFileToTree(RelativeFilename, Entry);

		if (File.Contains(TEXT("DevelopmentAssetRegistry.bin")))
		{
			PakFileSumary.AssetRegistryPath = File;
		}
	}

	PakFileSumary.PakFileSize = TotalSize;

	RefreshTreeNode(TreeRoot);
	RefreshTreeNodeSizePercent(TreeRoot);
	ParseAssetFile(TreeRoot);

	bHasPakLoaded = true;

	if (!PakFileSumary.AssetRegistryPath.IsEmpty())
	{
		LoadAssetRegistry(PakFileSumary.AssetRegistryPath);
	}

	UE_LOG(LogPakAnalyzer, Log, TEXT("Finish load pak file: %s."), *InPakPath);

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

		AssetParseWorker->StartParse(UAssetFiles, PakFileSumary.PakFilePath, PakFileSumary.PakInfo.Version, CachedAESKey);
	}
}

void FFolderAnalyzer::InitializeAssetParseWorker()
{
	UE_LOG(LogPakAnalyzer, Log, TEXT("Initialize asset parse worker."));

	if (!AssetParseWorker.IsValid())
	{
		AssetParseWorker = MakeShared<FAssetParseThreadWorker>();
		AssetParseWorker->OnReadAssetContent.BindRaw(this, &FFolderAnalyzer::OnReadAssetContent);
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

	const FString FilePath = PakFileSumary.MountPoint / InFile->Path;
	bOutSuccess = FFileHelper::LoadFileToArray(OutContent, *FilePath);
}
