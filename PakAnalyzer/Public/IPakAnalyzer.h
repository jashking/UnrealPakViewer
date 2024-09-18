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

	virtual bool LoadPakFiles(const TArray<FString>& InPakPaths, const TArray<FString>& InDefaultAESKeys, int32 ContainerStartIndex = 0) = 0;
	virtual void GetFiles(const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, const TMap<int32, bool>& InPakIndexFilter, TArray<FPakFileEntryPtr>& OutFiles) const = 0;
	virtual const TArray<FPakFileSumaryPtr>& GetPakFileSumary() const = 0;
	virtual const TArray<FPakTreeEntryPtr>& GetPakTreeRootNode() const = 0;
	virtual void ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles) = 0;
	virtual void CancelExtract() = 0;
	virtual bool ExportToJson(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles) = 0;
	virtual bool ExportToCsv(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles) = 0;
	virtual void SetExtractThreadCount(int32 InThreadCount) = 0;
	virtual bool LoadAssetRegistry(const FString& InRegristryPath) = 0;
	virtual FString GetAssetRegistryPath() const = 0;
};
