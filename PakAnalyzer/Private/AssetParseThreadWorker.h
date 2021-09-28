#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Misc/AES.h"

#include "Misc/Guid.h"
#include "PakFileEntry.h"

typedef TMap<FName, FName> ClassTypeMap;
DECLARE_DELEGATE_ThreeParams(FOnReadAssetContent, FPakFileEntryPtr /*InFile*/, bool& /*bOutSuccess*/, TArray<uint8>& /*OutContent*/);
DECLARE_DELEGATE_TwoParams(FOnParseFinish, bool/* bCancel*/, const ClassTypeMap&/* ClassMap*/);

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
	void StartParse(TArray<FPakFileEntryPtr>& InFiles, TArray<FPakFileSumary>& InSummaries);

	FOnReadAssetContent OnReadAssetContent;
	FOnParseFinish OnParseFinish;

protected:
	bool ParseObjectName(const TArray<FObjectImport>& Imports, const TArray<FObjectExport>& Exports, FPackageIndex Index, FName& OutObjectName);
	bool ParseObjectPath(FAssetSummaryPtr InSummary, FPackageIndex Index, FName& OutFullPath);

protected:
	class FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;

	TArray<FPakFileEntryPtr> Files;
	TArray<FPakFileSumary> Summaries;
};
