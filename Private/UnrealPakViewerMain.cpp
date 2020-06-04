// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UnrealPakViewerMain.h"

#include "RequiredProgramMainCPPInclude.h"

IMPLEMENT_APPLICATION(UnrealPakViewer, "UnrealPakViewer");

/**
 * Platform agnostic implementation of the main entry point.
 */
int32 UnrealPakViewerMain(const TCHAR* CommandLine)
{
	// Override the stack size for the thread pool.
	FQueuedThreadPool::OverrideStackSize = 256 * 1024;

	//im: ???
	FCommandLine::Set(CommandLine);

	// Initialize core.
	GEngineLoop.PreInit(CommandLine);

	// Tell the module manager it may now process newly-loaded UObjects when new C++ modules are loaded.
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	// Shut down.
	//im: ??? FCoreDelegates::OnExit.Broadcast();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppPreExit(); //im: ???

#if STATS
	FThreadStats::StopThread();
#endif

	FTaskGraphInterface::Shutdown(); //im: ???

	RequestEngineExit(TEXT("requestQuit"));

	return 0;
}
