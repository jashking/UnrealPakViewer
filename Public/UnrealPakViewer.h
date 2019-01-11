// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

// Pre-compiled header includes
#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "StandaloneRenderer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUnrealPakViewer, Log, All)

/**
* Run the crash report client app
*/
void RunUnrealPakViewer(const TCHAR* Commandline);