#include "UnrealPakViewerApplication.h"

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "StandaloneRenderer.h"
#include "Stats/Stats2.h"
#include "Styling/CoreStyle.h"

#include "UnrealPakViewerStyle.h"
#include "Widgets/SMainWindow.h"
#include "OodleModule.h"

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
		FTicker::GetCoreTicker().Tick(DeltaTime);

		// Throttle frame rate.
		FPlatformProcess::Sleep(FMath::Max<float>(0.0f, IdealFrameTime - (FPlatformTime::Seconds() - LastTime)));

		double CurrentTime = FPlatformTime::Seconds();
		DeltaTime = CurrentTime - LastTime;
		LastTime = CurrentTime;

		FStats::AdvanceFrame(false);

		GLog->FlushThreadedLogs(); //im: ???
	}

	ShutdownApplication();
}

void FUnrealPakViewerApplication::InitializeApplication()
{
	FCoreStyle::ResetToDefault();
	FUnrealPakViewerStyle::Initialize();

	// Load required modules.
	FModuleManager::Get().LoadModuleChecked("EditorStyle");

	// Load plug-ins.
	// @todo: allow for better plug-in support in standalone Slate applications
	IPluginManager::Get().LoadModulesForEnabledPlugins(ELoadingPhase::PreDefault);

	// Load optional modules.
	FModuleManager::Get().LoadModule("SettingsEditor");

	// Load oodle module.
	IOodleModule::Get();

	// Crank up a normal Slate application using the platform's standalone renderer.
	FSlateApplication::InitializeAsStandaloneApplication(GetStandardStandaloneRenderer());

	// Menu anims aren't supported. See Runtime\Slate\Private\Framework\Application\MenuStack.cpp.
	FSlateApplication::Get().EnableMenuAnimations(false);

	FSlateApplication::Get().AddWindow(SNew(SMainWindow));
}

void FUnrealPakViewerApplication::ShutdownApplication()
{
	// Shut down application.
	FSlateApplication::Shutdown();

	FUnrealPakViewerStyle::Shutdown();
}
