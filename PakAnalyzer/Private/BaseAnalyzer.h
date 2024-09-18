// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HAL/CriticalSection.h"
#include "Misc/AES.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"

#include "IPakAnalyzer.h"

class FArrayReader;

class FBaseAnalyzer : public IPakAnalyzer
{
public:
	FBaseAnalyzer();
	virtual ~FBaseAnalyzer();

	virtual bool LoadPakFiles(const TArray<FString>& InPakPaths, const TArray<FString>& InDefaultAESKeys, int32 ContainerStartIndex = 0) override;
	virtual void GetFiles(const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, const TMap<int32, bool>& InPakIndexFilter, TArray<FPakFileEntryPtr>& OutFiles) const override;
	virtual const TArray<FPakFileSumaryPtr>& GetPakFileSumary() const override;
	virtual const TArray<FPakTreeEntryPtr>& GetPakTreeRootNode() const override;
	virtual bool LoadAssetRegistry(const FString& InRegristryPath) override;
	virtual bool ExportToJson(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles) override;
	virtual bool ExportToCsv(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles) override;
	virtual FString GetAssetRegistryPath() const override;
	virtual void ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles) override {}
	virtual void CancelExtract() override {}
	virtual void SetExtractThreadCount(int32 InThreadCount) override {}

protected:
	virtual void Reset();
	virtual FString ResolveCompressionMethod(const FPakFileSumary& Summary, const FPakEntry* InPakEntry) const;

	FPakTreeEntryPtr InsertFileToTree(FPakTreeEntryPtr InRoot, const FPakFileSumary& Summary, const FString& InFullPath, const FPakEntry& InPakEntry);
	bool LoadAssetRegistry(FArrayReader& InData);
	void RefreshPackageDependency(FPakTreeEntryPtr InTreeRoot, FPakTreeEntryPtr InRoot);
	void RefreshClassMap(FPakTreeEntryPtr InTreeRoot, FPakTreeEntryPtr InRoot);
	void RefreshTreeNode(FPakTreeEntryPtr InRoot);
	void RefreshTreeNodeSizePercent(FPakTreeEntryPtr InTreeRoot, FPakTreeEntryPtr InRoot);
	void RetriveFiles(FPakTreeEntryPtr InRoot, const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, const TMap<int32, bool>& InPakIndexFilter, TArray<FPakFileEntryPtr>& OutFiles) const;
	void RetriveUAssetFiles(FPakTreeEntryPtr InRoot, TArray<FPakFileEntryPtr>& OutFiles) const;
	void InsertClassInfo(FPakTreeEntryPtr InTreeRoot, FPakTreeEntryPtr InRoot, FName InClassName, int32 InFileCount, int64 InSize, int64 InCompressedSize);
	FName GetAssetClass(const FString& InFilename, const FName InPackagePath);
	FName GetPackagePath(const FString& InFilePath);

protected:
	FCriticalSection CriticalSection;

	TArray<FPakFileSumaryPtr> PakFileSummaries;
	TArray<FPakTreeEntryPtr> PakTreeRoots;
	TMap<FName, FName> DefaultClassMap;

	FString AssetRegistryPath;

	TSharedPtr<class FAssetRegistryState> AssetRegistryState;
};
