// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "IPakAnalyzer.h"

class FPakAnalyzer : public IPakAnalyzer, public TSharedFromThis<FPakAnalyzer>
{
public:
	FPakAnalyzer();
	virtual ~FPakAnalyzer();

	virtual bool LoadPakFile(const FString& InPakPath) override;

protected:
	TSharedPtr<class FPakFile> PakFile;
};
