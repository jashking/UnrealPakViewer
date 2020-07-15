// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPakAnalyzer, Log, All);

DECLARE_DELEGATE_RetVal(FString, FOnGetAESKey);
DECLARE_DELEGATE_OneParam(FOnLoadPakFailed, const FString&)

class FPakAnalyzerDelegates
{
public:
	static FOnGetAESKey OnGetAESKey;
	static FOnLoadPakFailed OnLoadPakFailed;
}; 
