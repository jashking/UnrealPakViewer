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
#include "UObject/PackageId.h"
#include "Templates/SharedPointer.h"

#include "BaseAnalyzer.h"
#include "IoStoreDefines.h"

class FIoStoreAnalyzer : public FBaseAnalyzer, public TSharedFromThis<FIoStoreAnalyzer>
{
public:
	FIoStoreAnalyzer();
	virtual ~FIoStoreAnalyzer();

	virtual bool LoadPakFile(const FString& InPakPath, const FString& InAESKey = TEXT("")) override;
	virtual void ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles) override;
	virtual void CancelExtract() override;
	virtual void SetExtractThreadCount(int32 InThreadCount) override;
	virtual const FPakFileSumary& GetPakFileSumary() const override;

protected:
	virtual void Reset() override;
	TSharedPtr<FIoStoreReader> CreateIoStoreReader(const FString& InPath);

	bool InitializeGlobalReader(const FString& InPakPath);
	bool InitializeReaders(const TArray<FString>& InPaks);
	bool PreLoadIoStore(const FString& InTocPath, const FString& InCasPath, TMap<FGuid, FAES::FAESKey>& OutKeys);
	bool TryDecryptIoStore(const FIoStoreTocResourceInfo& TocResource, const FIoOffsetAndLength& OffsetAndLength, const FIoStoreTocEntryMeta& Meta, const FString& InCasPath, const FString& InKey, FAES::FAESKey& OutAESKey);
	bool FillPackageInfo(uint64 ContainerId, FStorePackageInfo& OutPackageInfo);
	void OnExtractFiles();
	void StopExtract();
	void UpdateExtractProgress(int32 InTotal, int32 InComplete, int32 InError);

protected:
	TSharedPtr<FIoStoreReader> GlobalIoStoreReader;
	TArray<FNameEntryId> GlobalNameMap;
	TArray<FContainerInfo> StoreContainers;
	TArray<FStorePackageInfo> PackageInfos;
	TMap<FString, int32> FileToPackageIndex;

	FString DefaultAESKey;

	TArray<int32> PendingExtracePackages;
	TArray<TFuture<void>> ExtractThread;
	FThreadSafeBool IsStopExtract;
	FString ExtractOutputPath;
};

#endif // ENABLE_IO_STORE_ANALYZER
