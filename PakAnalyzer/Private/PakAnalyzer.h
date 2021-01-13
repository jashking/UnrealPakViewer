// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HAL/CriticalSection.h"
#include "IPlatformFilePak.h"
#include "Misc/AES.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "Serialization/ArrayReader.h"

#include "IPakAnalyzer.h"

struct FPakEntry;

class FPakAnalyzer : public IPakAnalyzer, public TSharedFromThis<FPakAnalyzer>
{
public:
	FPakAnalyzer();
	virtual ~FPakAnalyzer();

	virtual bool LoadPakFile(const FString& InPakPath) override;
	virtual int32 GetFileCount() const override;
	virtual void GetFiles(const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, TArray<FPakFileEntryPtr>& OutFiles) const override;
	virtual FString GetLastLoadGuid() const override;
	virtual bool IsLoadDirty(const FString& InGuid) const override;
	virtual const FPakFileSumary& GetPakFileSumary() const override;
	virtual FPakTreeEntryPtr GetPakTreeRootNode() const override;
	virtual void ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles) override;
	virtual void CancelExtract() override;
	virtual bool ExportToJson(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles) override;
	virtual bool ExportToCsv(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles) override;
	virtual bool HasPakLoaded() const override;
	virtual void SetExtractThreadCount(int32 InThreadCount) override;
	virtual bool LoadAssetRegistry(const FString& InRegristryPath) override;

protected:
	void Reset();

	FString ResolveCompressionMethod(const FPakEntry* InPakEntry) const;

	FPakTreeEntryPtr InsertFileToTree(const FString& InFullPath, const FPakEntry& InPakEntry);
	void RefreshTreeNode(FPakTreeEntryPtr InRoot);
	void RefreshTreeNodeSizePercent(FPakTreeEntryPtr InRoot);
	void RetriveFiles(FPakTreeEntryPtr InRoot, const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, TArray<FPakFileEntryPtr>& OutFiles) const;
	void RefreshClassMap(FPakTreeEntryPtr InRoot);
	void InsertClassInfo(FPakTreeEntryPtr InRoot, FName InClassName, int32 InFileCount, int64 InSize, int64 InCompressedSize);
	FName GetAssetClass(const FString& InFilename);

	bool LoadAssetRegistryFromPak(FPakFile* InPakFile, FPakFileEntryPtr InPakFileEntry);
	bool LoadAssetRegistry(FArrayReader& InData);

	bool PreLoadPak(const FString& InPakPath);
	bool ValidateEncryptionKey(TArray<uint8>& IndexData, const FSHAHash& InExpectedHash, const FAES::FAESKey& InAESKey);

	void InitializeExtractWorker();
	void ShutdownAllExtractWorker();

	// Extract progress
	void OnUpdateExtractProgress(const FGuid& WorkerGuid, int32 CompleteCount, int32 ErrorCount, int32 TotalCount);

	struct FExtractProgress
	{
		int32 CompleteCount;
		int32 ErrorCount;
		int32 TotalCount;
	};

	void ResetProgress();

protected:
	FCriticalSection CriticalSection;

	FPakTreeEntryPtr TreeRoot;

	FGuid LoadGuid;

	FPakFileSumary PakFileSumary;

	int32 ExtractWorkerCount;
	TArray<TSharedPtr<class FExtractThreadWorker>> ExtractWorkers;
	TMap<FGuid, FExtractProgress> ExtractWorkerProgresses;

	FAES::FAESKey CachedAESKey;

	bool bHasPakLoaded;

	TSharedPtr<class FAssetRegistryState> AssetRegistryState;
};
