// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FPakAnalyzer : public TSharedFromThis<FPakAnalyzer>
{
public:
	FPakAnalyzer();
	virtual ~FPakAnalyzer();

	/** @return the global instance of the main manager (FPakAnalyzer). */
	static TSharedPtr<FPakAnalyzer> Get();
	static void Initialize();
	void Shutdown();

	bool LoadPakFile(const FString& InPakPath);

protected:
	TSharedPtr<class FPakFile> PakFile;

private:
	/** A shared pointer to the global instance of the main manager (FPakAnalyzer). */
	static TSharedPtr<FPakAnalyzer> Instance;
};
