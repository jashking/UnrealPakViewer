// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

#define ENABLE_IO_STORE_ANALYZER 1

DECLARE_LOG_CATEGORY_EXTERN(LogPakAnalyzer, Log, All);

class FPakAnalyzerDelegates
{
public:
	DECLARE_DELEGATE_RetVal_ThreeParams(FString, FOnGetAESKey, const FString&/* PakPath*/, const FGuid&/* Guid*/, bool& /*bCancel*/);
	DECLARE_DELEGATE_OneParam(FOnLoadPakFailed, const FString&)
	DECLARE_DELEGATE_ThreeParams(FOnUpdateExtractProgress, int32 /*CompleteCount*/, int32 /*ErrorCount*/, int32 /*TotalCount*/);
	DECLARE_DELEGATE(FOnExtractStart);
	DECLARE_MULTICAST_DELEGATE(FOnAssetParseFinish);
	DECLARE_MULTICAST_DELEGATE(FOnPakLoadFinish);

public:
	static FOnGetAESKey OnGetAESKey;
	static FOnLoadPakFailed OnLoadPakFailed;
	static FOnUpdateExtractProgress OnUpdateExtractProgress;
	static FOnExtractStart OnExtractStart;
	static FOnAssetParseFinish OnAssetParseFinish;
	static FOnPakLoadFinish OnPakLoadFinish;
};
