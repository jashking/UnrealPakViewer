// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PakFileEntry.h"

class IPakAnalyzer
{
public:
	IPakAnalyzer() {}
	virtual ~IPakAnalyzer() {}

	virtual bool LoadPakFile(const FString& InPakPath) = 0;
	virtual int32 GetFileCount() const = 0;
	virtual void GetFiles(const FString& InFilterText, TArray<FPakFileEntryPtr>& OutFiles) const = 0;
	virtual FString GetLastLoadGuid() const = 0;
	virtual bool IsLoadDirty(const FString& InGuid) const = 0;
	virtual const FPakFileSumary& GetPakFileSumary() const = 0;
	virtual FPakTreeEntryPtr GetPakTreeRootNode() const = 0;
	virtual FString ResolveCompressionMethod(int32 InMethod) const = 0;
};
