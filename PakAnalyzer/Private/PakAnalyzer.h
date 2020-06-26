// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/Guid.h"

#include "IPakAnalyzer.h"

struct FPakEntry;

class FPakAnalyzer : public IPakAnalyzer, public TSharedFromThis<FPakAnalyzer>
{
public:
	FPakAnalyzer();
	virtual ~FPakAnalyzer();

	virtual bool LoadPakFile(const FString& InPakPath) override;
	virtual int32 GetFileCount() const override;
	virtual const TArray<FPakFileEntryPtr>& GetFiles() const override;
	virtual FString GetLastLoadGuid() const override;
	virtual bool IsLoadDirty(const FString& InGuid) const override;

protected:
	void Reset();

protected:
	TArray<FPakFileEntryPtr> Files;

	TSharedPtr<class FPakFile> PakFile;

	FGuid LoadGuid;
};
