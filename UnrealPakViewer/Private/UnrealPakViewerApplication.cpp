#include "UnrealPakViewerApplication.h"

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Launch/Resources/Version.h"
#include "Modules/ModuleManager.h"
#include "StandaloneRenderer.h"
#include "Stats/Stats2.h"
#include "Styling/CoreStyle.h"

#include "UnrealPakViewerStyle.h"
#include "Widgets/SMainWindow.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define IDEAL_FRAMERATE 60

////////////////////////////////////////////////////////////////////////////////////////////////////

void FUnrealPakViewerApplication::Exec()
{
	InitializeApplication();

	// Enter main loop.
	double DeltaTime = 0.0;
	double LastTime = FPlatformTime::Seconds();
	const float IdealFrameTime = 1.0f / IDEAL_FRAMERATE;

	while (!IsEngineExitRequested())
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);

		FSlateApplication::Get().PumpMessages();
		FSlateApplication::Get().Tick();
#if ENGINE_MAJOR_VERSION >= 5
		FTSTicker::GetCoreTicker().Tick(DeltaTime);
#else
		FTicker::GetCoreTicker().Tick(DeltaTime);
#endif

		// Throttle frame rate.
		FPlatformProcess::Sleep(FMath::Max<float>(0.0f, IdealFrameTime - (FPlatformTime::Seconds() - LastTime)));

		double CurrentTime = FPlatformTime::Seconds();
		DeltaTime = CurrentTime - LastTime;
		LastTime = CurrentTime;

		FStats::AdvanceFrame(false);

		FCoreDelegates::OnEndFrame.Broadcast();
		GLog->FlushThreadedLogs(); //im: ???

		GFrameCounter++;
	}

	ShutdownApplication();
}

void FUnrealPakViewerApplication::InitializeApplication()
{
	//FCoreStyle::ResetToDefault();

	// Crank up a normal Slate application using the platform's standalone renderer.
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());

	// Load required modules.
	//FModuleManager::Get().LoadModuleChecked("EditorStyle");

	// Load plug-ins.
	// @todo: allow for better plug-in support in standalone Slate applications
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault);
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::Default);

	// Load optional modules.
	if (FModuleManager::Get().ModuleExists(TEXT("SettingsEditor")))
	{
		FModuleManager::Get().LoadModule("SettingsEditor");
	}

#if ENGINE_MAJOR_VERSION <= 4
	// Menu anims aren't supported. See Runtime\Slate\Private\Framework\Application\MenuStack.cpp.
	FSlateApplication::Get().EnableMenuAnimations(false);
#endif

	FSlateApplication::InitHighDPI(true);
	FUnrealPakViewerStyle::Initialize();

	FSlateApplication::Get().AddWindow(SNew(SMainWindow));
}

void FUnrealPakViewerApplication::ShutdownApplication()
{
	FUnrealPakViewerStyle::Shutdown();

	// Shut down application.
	FSlateApplication::Shutdown();
}
