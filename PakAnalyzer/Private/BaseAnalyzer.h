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

	virtual void GetFiles(const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, TArray<FPakFileEntryPtr>& OutFiles) const override;
	virtual int32 GetFileCount() const override;
	virtual FString GetLastLoadGuid() const override;
	virtual bool IsLoadDirty(const FString& InGuid) const override;
	virtual const FPakFileSumary& GetPakFileSumary() const override;
	virtual FPakTreeEntryPtr GetPakTreeRootNode() const override;
	virtual bool HasPakLoaded() const override;
	virtual bool LoadAssetRegistry(const FString& InRegristryPath) override;
	virtual bool ExportToJson(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles) override;
	virtual bool ExportToCsv(const FString& InOutputPath, const TArray<FPakFileEntryPtr>& InFiles) override;
	virtual FString GetDecriptionAESKey() const override;

protected:
	virtual void Reset();
	virtual FString ResolveCompressionMethod(const FPakEntry* InPakEntry) const;

	FPakTreeEntryPtr InsertFileToTree(const FString& InFullPath, const FPakEntry& InPakEntry);
	bool LoadAssetRegistry(FArrayReader& InData);
	void RefreshClassMap(FPakTreeEntryPtr InRoot);
	void RefreshTreeNode(FPakTreeEntryPtr InRoot);
	void RefreshTreeNodeSizePercent(FPakTreeEntryPtr InRoot);
	void RetriveFiles(FPakTreeEntryPtr InRoot, const FString& InFilterText, const TMap<FName, bool>& InClassFilterMap, TArray<FPakFileEntryPtr>& OutFiles) const;
	void RetriveUAssetFiles(FPakTreeEntryPtr InRoot, TArray<FPakFileEntryPtr>& OutFiles) const;
	void InsertClassInfo(FPakTreeEntryPtr InRoot, FName InClassName, int32 InFileCount, int64 InSize, int64 InCompressedSize);
	FName GetAssetClass(const FString& InFilename, const FName InPackagePath);
	FName GetPackagePath(const FString& InFilePath);

protected:
	FCriticalSection CriticalSection;

	FPakFileSumary PakFileSumary;
	FGuid LoadGuid;
	FAES::FAESKey CachedAESKey;

	FPakTreeEntryPtr TreeRoot;

	bool bHasPakLoaded;

	TSharedPtr<class FAssetRegistryState> AssetRegistryState;
};
