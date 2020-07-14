// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HAL/CriticalSection.h"
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
	virtual void GetFiles(const FString& InFilterText, TArray<FPakFileEntryPtr>& OutFiles) const override;
	virtual FString GetLastLoadGuid() const override;
	virtual bool IsLoadDirty(const FString& InGuid) const override;
	virtual const FPakFileSumary& GetPakFileSumary() const override;
	virtual FPakTreeEntryPtr GetPakTreeRootNode() const override;

protected:
	void Reset();

	FPakTreeEntryPtr InsertTreeNode(const FString& InFullPath, const FPakEntry* InEntry, bool bIsDirectory);
	void RefreshTreeNode(FPakTreeEntryPtr InRoot);
	void RetriveFiles(FPakTreeEntryPtr InRoot, const FString& InFilterText, TArray<FPakFileEntryPtr>& OutFiles) const;

protected:
	FCriticalSection CriticalSection;

	FPakTreeEntryPtr TreeRoot;

	TSharedPtr<class FPakFile> PakFile;

	FGuid LoadGuid;

	FPakFileSumary PakFileSumary;
};
