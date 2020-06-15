// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IPakAnalyzer
{
public:
	IPakAnalyzer() {}
	virtual ~IPakAnalyzer() {}

	virtual bool LoadPakFile(const FString& InPakPath) = 0;
};
