// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "CommonDefines.h"

#if ENABLE_IO_STORE_ANALYZER

#include "Async/Async.h"
#include "HAL/ThreadSafeBool.h"
#include "IO/IoDispatcher.h"
#include "Misc/AES.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "IO/PackageId.h"
#include "Templates/SharedPointer.h"

#include "BaseAnalyzer.h"
#include "IoStoreDefines.h"

class FIoStoreAnalyzer : public FBaseAnalyzer, public TSharedFromThis<FIoStoreAnalyzer>
{
public:
	FIoStoreAnalyzer();
	virtual ~FIoStoreAnalyzer();

	virtual bool LoadPakFiles(const TArray<FString>& InPakPaths, const TArray<FString>& InDefaultAESKeys, int32 ContainerStartIndex = 0) override;
	virtual void ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles) override;
	virtual void CancelExtract() override;
	virtual void SetExtractThreadCount(int32 InThreadCount) override;
	virtual void Reset() override;
	
protected:
	TSharedPtr<FIoStoreReader> CreateIoStoreReader(const FString& InPath, const FString& InDefaultAESKey, FString& OutDecryptKey);

	bool InitializeGlobalReader(const FString& InPakPath);
	bool InitializeReaders(const TArray<FString>& InPaks, const TArray<FString>& InDefaultAESKeys);
	bool PreLoadIoStore(const FString& InTocPath, const FString& InCasPath, const FString& InDefaultAESKey, TMap<FGuid, FAES::FAESKey>& OutKeys, FString& OutDecryptKey);
	bool TryDecryptIoStore(const FIoStoreTocResourceInfo& TocResource, const FIoOffsetAndLength& OffsetAndLength, const FIoStoreTocEntryMeta& Meta, const FString& InCasPath, const FString& InKey, FAES::FAESKey& OutAESKey);
	bool FillPackageInfo(const FIoStoreTocResourceInfo& TocResource, FStorePackageInfo& OutPackageInfo);
	void OnExtractFiles();
	void StopExtract();
	void UpdateExtractProgress(int32 InTotal, int32 InComplete, int32 InError);
	void ParseChunkInfo(const FIoChunkId& InChunkId, FPackageId& OutPackageId, EIoChunkType& OutChunkType);
	FName FindObjectName(FPackageObjectIndex Index, const FStorePackageInfo* PackageInfo);

protected:
	TSharedPtr<FIoStoreReader> GlobalIoStoreReader;
	TArray<FDisplayNameEntryId> GlobalNameMap;
	TArray<FContainerInfo> StoreContainers;
	TArray<FStorePackageInfo> PackageInfos;
	TMap<FString, int32> FileToPackageIndex;

	TArray<FString> DefaultAESKeys;

	TArray<int32> PendingExtracePackages;
	TArray<TFuture<void>> ExtractThread;
	FThreadSafeBool IsStopExtract;
	FString ExtractOutputPath;

	TMap<FPackageObjectIndex, FScriptObjectDesc> ScriptObjectByGlobalIdMap;
	TMap<uint64, FIoStoreTocResourceInfo> TocResources;
	TMap<FPackageObjectIndex, const FIoStoreExport*> ExportByGlobalIdMap;
};

#endif // ENABLE_IO_STORE_ANALYZER
