#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Misc/AES.h"

#include "Misc/Guid.h"
#include "PakFileEntry.h"

DECLARE_DELEGATE_ThreeParams(FOnReadAssetContent, FPakFileEntryPtr /*InFile*/, bool& /*bOutSuccess*/, TArray<uint8>& /*OutContent*/);

class FAssetParseThreadWorker : public FRunnable
{
public:
	FAssetParseThreadWorker();
	~FAssetParseThreadWorker();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	void Shutdown();
	void EnsureCompletion();
	void StartParse(TArray<FPakFileEntryPtr>& InFiles, const FString& InPakFile, int32 InPakVersion, const FAES::FAESKey& InKey);

	FOnReadAssetContent OnReadAssetContent;

protected:
	bool ParseObjectName(FAssetSummaryPtr InSummary, FPackageIndex Index, FString& OutObjectName);
	bool ParseObjectPath(FAssetSummaryPtr InSummary, FPackageIndex Index, FString& OutFullPath);

protected:
	class FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;

	TArray<FPakFileEntryPtr> Files;

	FString PakFilePath;
	int32 PakVersion;
	FAES::FAESKey AESKey;
};
