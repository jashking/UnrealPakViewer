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

	bool LoadPakFile(const FString& InPakPath);

	void Shutdown();

protected:
	TSharedPtr<class FPakFile> PakFile;

private:
	/** A shared pointer to the global instance of the main manager (FPakAnalyzer). */
	static TSharedPtr<FPakAnalyzer> Instance;
};
