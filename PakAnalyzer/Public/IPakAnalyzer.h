// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PakFileEntry.h"

struct FPakEntry;

static const int32 DEFAULT_EXTRACT_THREAD_COUNT = 4;

class IPakAnalyzer
{
public:
	IPakAnalyzer() {}
	virtual ~IPakAnalyzer() {}

	virtual bool LoadPakFile(const FString& InPakPath) = 0;
	virtual int32 GetFileCount() const = 0;
	virtual void GetFiles(const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, TArray<FPakFileEntryPtr>& OutFiles) const = 0;
	virtual FString GetLastLoadGuid() const = 0;
	virtual bool IsLoadDirty(const FString& InGuid) const = 0;
	virtual const FPakFileSumary& GetPakFileSumary() const = 0;
	virtual FPakTreeEntryPtr GetPakTreeRootNode() const = 0;
	virtual void ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles) = 0;
	virtual void CancelExtract() = 0;
	virtual bool ExportToJson(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles) = 0;
	virtual bool ExportToCsv(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles) = 0;
	virtual bool HasPakLoaded() const = 0;
	virtual void SetExtractThreadCount(int32 InThreadCount) = 0;
	virtual bool LoadAssetRegistry(const FString& InRegristryPath) = 0;
};
