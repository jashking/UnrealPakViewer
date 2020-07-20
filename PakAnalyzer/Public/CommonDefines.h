// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPakAnalyzer, Log, All);

class FPakAnalyzerDelegates
{
public:
	DECLARE_DELEGATE_RetVal(FString, FOnGetAESKey);
	DECLARE_DELEGATE_OneParam(FOnLoadPakFailed, const FString&)
	DECLARE_DELEGATE_ThreeParams(FOnUpdateExtractProgress, int32 /*CompleteCount*/, int32 /*ErrorCount*/, int32 /*TotalCount*/);
	DECLARE_DELEGATE(FOnExtractStart);

public:
	static FOnGetAESKey OnGetAESKey;
	static FOnLoadPakFailed OnLoadPakFailed;
	static FOnUpdateExtractProgress OnUpdateExtractProgress;
	static FOnExtractStart OnExtractStart;
}; 
