// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UnrealPakViewerMain.h"

#include "RequiredProgramMainCPPInclude.h"

#include "UnrealPakViewerApplication.h"

IMPLEMENT_APPLICATION(UnrealPakViewer, "UnrealPakViewer");

/**
 * Platform agnostic implementation of the main entry point.
 */
int32 UnrealPakViewerMain(const TCHAR* CommandLine)
{
#if defined(ParentProjectName)
	const FString ProjectDir = FString::Printf(TEXT("../../../%s/Programs/%s/"), ParentProjectName, FApp::GetProjectName());
	FPlatformMisc::SetOverrideProjectDir(ProjectDir);
#endif

	// Override the stack size for the thread pool.
	FQueuedThreadPool::OverrideStackSize = 256 * 1024;

	//im: ???
	FCommandLine::Set(CommandLine);

	// Initialize core.
	GEngineLoop.PreInit(CommandLine);

	// Tell the module manager it may now process newly-loaded UObjects when new C++ modules are loaded.
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	// Run application
	FUnrealPakViewerApplication::Exec();

	// Shut down.
	FEngineLoop::AppPreExit(); //im: ???

	FModuleManager::Get().UnloadModulesAtShutdown();
	

#if STATS
	FThreadStats::StopThread();
#endif

	FTaskGraphInterface::Shutdown(); //im: ???

	FEngineLoop::AppExit();

	return 0;
}
