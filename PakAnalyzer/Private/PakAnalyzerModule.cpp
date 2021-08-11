// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PakAnalyzerModule.h"
#include "Modules/ModuleManager.h"

#include "CommonDefines.h"
#include "PakAnalyzer.h"
#include "IoStoreAnalyzer.h"

DEFINE_LOG_CATEGORY(LogPakAnalyzer);

FPakAnalyzerDelegates::FOnGetAESKey FPakAnalyzerDelegates::OnGetAESKey;
FPakAnalyzerDelegates::FOnLoadPakFailed FPakAnalyzerDelegates::OnLoadPakFailed;
FPakAnalyzerDelegates::FOnUpdateExtractProgress FPakAnalyzerDelegates::OnUpdateExtractProgress;
FPakAnalyzerDelegates::FOnExtractStart FPakAnalyzerDelegates::OnExtractStart;

class FPakAnalyzerModule : public IPakAnalyzerModule
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual void InitializeAnalyzerBackend(const FString& InType = TEXT("pak")) override;
	virtual IPakAnalyzer* GetPakAnalyzer() override;

protected:
	TSharedPtr<IPakAnalyzer> AnalyzerInstance;
};

IMPLEMENT_MODULE(FPakAnalyzerModule, PakAnalyzer);

void FPakAnalyzerModule::StartupModule()
{
}

void FPakAnalyzerModule::ShutdownModule()
{
	AnalyzerInstance.Reset();
}

void FPakAnalyzerModule::InitializeAnalyzerBackend(const FString& InType)
{
	if (InType.Equals(TEXT("folder")))
	{
		AnalyzerInstance = MakeShared<FPakAnalyzer>();
	}
#if ENABLE_IO_STORE_ANALYZER
	else if (InType.Equals(TEXT("ucas")) || InType.Equals(TEXT("utoc")))
	{
		AnalyzerInstance = MakeShared<FIoStoreAnalyzer>();
	}
#endif // ENABLE_IO_STORE_ANALYZER
	else
	{
		AnalyzerInstance = MakeShared<FPakAnalyzer>();
	}
}

IPakAnalyzer* FPakAnalyzerModule::GetPakAnalyzer()
{
	return AnalyzerInstance.IsValid() ? AnalyzerInstance.Get() : nullptr;
}
