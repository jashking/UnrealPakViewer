// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PakAnalyzerModule.h"
#include "Modules/ModuleManager.h"

#include "CommonDefines.h"
#include "PakAnalyzer.h"

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

	virtual IPakAnalyzer* GetPakAnalyzer() override;

protected:
	TSharedPtr<FPakAnalyzer> AnalyzerInstance;
};

IMPLEMENT_MODULE(FPakAnalyzerModule, PakAnalyzer);

void FPakAnalyzerModule::StartupModule()
{
	AnalyzerInstance = MakeShared<FPakAnalyzer>();
}

void FPakAnalyzerModule::ShutdownModule()
{
	AnalyzerInstance.Reset();
}

IPakAnalyzer* FPakAnalyzerModule::GetPakAnalyzer()
{
	return AnalyzerInstance.Get();
}
