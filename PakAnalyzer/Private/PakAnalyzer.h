// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HAL/CriticalSection.h"
#include "IPlatformFilePak.h"
#include "Misc/AES.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "Serialization/ArrayReader.h"

#include "BaseAnalyzer.h"

struct FPakEntry;

class FPakAnalyzer : public FBaseAnalyzer, public TSharedFromThis<FPakAnalyzer>
{
public:
	FPakAnalyzer();
	virtual ~FPakAnalyzer();

	virtual bool LoadPakFile(const FString& InPakPath) override;
	virtual void ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles) override;
	virtual void CancelExtract() override;
	virtual void SetExtractThreadCount(int32 InThreadCount) override;

protected:
	bool LoadAssetRegistryFromPak(FPakFile* InPakFile, FPakFileEntryPtr InPakFileEntry);

	bool PreLoadPak(const FString& InPakPath);
	bool ValidateEncryptionKey(TArray<uint8>& IndexData, const FSHAHash& InExpectedHash, const FAES::FAESKey& InAESKey);

	void InitializeExtractWorker();
	void ShutdownAllExtractWorker();

	void ParseAssetFile(FPakTreeEntryPtr InRoot);
	void InitializeAssetParseWorker();
	void ShutdownAssetParseWorker();

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
	int32 ExtractWorkerCount;
	TArray<TSharedPtr<class FExtractThreadWorker>> ExtractWorkers;
	TMap<FGuid, FExtractProgress> ExtractWorkerProgresses;

	TSharedPtr<class FAssetParseThreadWorker> AssetParseWorker;
};
