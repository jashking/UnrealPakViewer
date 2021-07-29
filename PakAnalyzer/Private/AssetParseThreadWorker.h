#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Misc/AES.h"

#include "Misc/Guid.h"
#include "PakFileEntry.h"

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

protected:
	class FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;

	TArray<FPakFileEntryPtr> Files;

	FString PakFilePath;
	int32 PakVersion;
	FAES::FAESKey AESKey;
};
