// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PakAnalyzerModule.h"

#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#include "BaseAnalyzer.h"
#include "CommonDefines.h"
#include "FolderAnalyzer.h"
#include "PakAnalyzer.h"
#include "IoStoreAnalyzer.h"
#include "UnrealAnalyzer.h"

DEFINE_LOG_CATEGORY(LogPakAnalyzer);

FPakAnalyzerDelegates::FOnGetAESKey FPakAnalyzerDelegates::OnGetAESKey;
FPakAnalyzerDelegates::FOnLoadPakFailed FPakAnalyzerDelegates::OnLoadPakFailed;
FPakAnalyzerDelegates::FOnUpdateExtractProgress FPakAnalyzerDelegates::OnUpdateExtractProgress;
FPakAnalyzerDelegates::FOnExtractStart FPakAnalyzerDelegates::OnExtractStart;
FPakAnalyzerDelegates::FOnAssetParseFinish FPakAnalyzerDelegates::OnAssetParseFinish;
FPakAnalyzerDelegates::FOnPakLoadFinish FPakAnalyzerDelegates::OnPakLoadFinish;

class FPakAnalyzerModule : public IPakAnalyzerModule
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual void InitializeAnalyzerBackend(const FString& InFullPath) override;
	virtual IPakAnalyzer* GetPakAnalyzer() override;

protected:
	TSharedPtr<IPakAnalyzer> AnalyzerInstance;
};

IMPLEMENT_MODULE(FPakAnalyzerModule, PakAnalyzer);

void FPakAnalyzerModule::StartupModule()
{
	AnalyzerInstance = MakeShared<FBaseAnalyzer>();
}

void FPakAnalyzerModule::ShutdownModule()
{
	AnalyzerInstance.Reset();
}

void FPakAnalyzerModule::InitializeAnalyzerBackend(const FString& InFullPath)
{
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();

	const FString Extension = FPaths::GetExtension(InFullPath);

	if (PlatformFile.DirectoryExists(*InFullPath))
	{
		AnalyzerInstance = MakeShared<FFolderAnalyzer>();
	}
	else
	{
		AnalyzerInstance = MakeShared<FUnrealAnalyzer>();
	}
}

IPakAnalyzer* FPakAnalyzerModule::GetPakAnalyzer()
{
	return AnalyzerInstance.IsValid() ? AnalyzerInstance.Get() : nullptr;
}
