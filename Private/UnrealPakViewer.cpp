// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "UnrealPakViewer.h"

#include "RequiredProgramMainCPPInclude.h"

#include "SUnrealPakViewer.h"

DEFINE_LOG_CATEGORY(LogUnrealPakViewer);
IMPLEMENT_APPLICATION(UnrealPakViewer, "UnrealPakViewer");

#define LOCTEXT_NAMESPACE "UnrealPakViewer"

/** Default main window size */
const FVector2D InitialWindowDimensions(800, 600);

/** Average tick rate the app aims for */
const float IdealTickRate = 30.f;

static void ExitUnrealPakViewer()
{
	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FTaskGraphInterface::Shutdown();

	FEngineLoop::AppExit();
}

static void Tick()
{
	static const float IdealFrameTime = 1.f / IdealTickRate;
	static double ActualDeltaTime = IdealFrameTime;
	static double LastTime = FPlatformTime::Seconds();

	// Tick app logic
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	FTicker::GetCoreTicker().Tick(ActualDeltaTime);

	// Tick SlateApplication
	//if (bTickSlate)
	{
		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();
	}

	// Sleep Throttling
	// Copied from Community Portal - should be shared
	FPlatformProcess::Sleep(FMath::Max<float>(0, IdealFrameTime - (FPlatformTime::Seconds() - LastTime)));

	// Calculate deltas
	const double AppTime = FPlatformTime::Seconds();
	ActualDeltaTime = AppTime - LastTime;
	LastTime = AppTime;
}

static void OnRequestExit()
{
	GIsRequestingExit = true;
}

bool RunWithUI()
{
	// create the platform slate application (what FSlateApplication::Get() returns)
	TSharedRef<FSlateApplication> Slate = FSlateApplication::Create(MakeShareable(FPlatformApplicationMisc::CreateApplication()));

	// initialize renderer
	TSharedRef<FSlateRenderer> SlateRenderer = GetStandardStandaloneRenderer();

	// Grab renderer initialization retry settings from ini
	int32 SlateRendererInitRetryCount = 10;
	double SlateRendererInitRetryInterval = 2.0;

	// Try to initialize the renderer. It's possible that we launched when the driver crashed so try a few times before giving up.
	bool bRendererInitialized = false;
	bool bRendererFailedToInitializeAtLeastOnce = false;
	do
	{
		SlateRendererInitRetryCount--;
		bRendererInitialized = FSlateApplication::Get().InitializeRenderer(SlateRenderer, true);
		if (!bRendererInitialized && SlateRendererInitRetryCount > 0)
		{
			bRendererFailedToInitializeAtLeastOnce = true;
			FPlatformProcess::Sleep(SlateRendererInitRetryInterval);
		}
	} while (!bRendererInitialized && SlateRendererInitRetryCount > 0);

	if (!bRendererInitialized)
	{
		// Close down the Slate application
		FSlateApplication::Shutdown();
		return false;
	}
	else if (bRendererFailedToInitializeAtLeastOnce)
	{
		// Wait until the driver is fully restored
		FPlatformProcess::Sleep(2.0f);

		// Update the display metrics
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
		FSlateApplication::Get().GetPlatformApplication()->OnDisplayMetricsChanged().Broadcast(DisplayMetrics);
	}

	// set the normal UE4 GIsRequestingExit when outer frame is closed
	FSlateApplication::Get().SetExitRequestedHandler(FSimpleDelegate::CreateStatic(&OnRequestExit));

	// open up the app window	
	TSharedRef<SUnrealPakViewer> MainContent = SNew(SUnrealPakViewer);

	const FSlateRect WorkArea = FSlateApplicationBase::Get().GetPreferredWorkArea();

	auto Window = FSlateApplication::Get().AddWindow(
		SNew(SWindow)
		.Title(NSLOCTEXT("UnrealPakViewer", "UnrealPakViewerAppName", "UnrealPakViewer"))
		.ClientSize(InitialWindowDimensions * FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(WorkArea.Left, WorkArea.Top))
		[
			MainContent
		]);

	// Setting focus seems to have to happen after the Window has been added
	FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);

	// loop until the app is ready to quit
	while (!GIsRequestingExit)
	{
		Tick();
	}

	// Close down the Slate application
	FSlateApplication::Shutdown();

	return true;
}

void RunUnrealPakViewer(const TCHAR* CommandLine)
{
	// Override the stack size for the thread pool.
	FQueuedThreadPool::OverrideStackSize = 256 * 1024;

	// Set up the main loop
	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogUnrealPakViewer, Display, TEXT("UnrealPakViewer Start."));

	// Make sure all UObject classes are registered and default properties have been initialized
	ProcessNewlyLoadedUObjects();

	// Tell the module manager is may now process newly-loaded UObjects when new C++ modules are loaded
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	RunWithUI();

	UE_LOG(LogUnrealPakViewer, Display, TEXT("UnrealPakViewer Exit."));
	ExitUnrealPakViewer();
}

#undef LOCTEXT_NAMESPACE
