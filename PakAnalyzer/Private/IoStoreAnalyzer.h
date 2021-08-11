// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "CommonDefines.h"

#if ENABLE_IO_STORE_ANALYZER

#include "IO/IoDispatcher.h"
#include "Misc/AES.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "UObject/PackageId.h"
#include "Templates/SharedPointer.h"

#include "BaseAnalyzer.h"

class FIoStoreAnalyzer : public FBaseAnalyzer, public TSharedFromThis<FIoStoreAnalyzer>
{
public:
	FIoStoreAnalyzer();
	virtual ~FIoStoreAnalyzer();

	virtual bool LoadPakFile(const FString& InPakPath) override;
	virtual void ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles) override;
	virtual void CancelExtract() override;
	virtual void SetExtractThreadCount(int32 InThreadCount) override;
	virtual const FPakFileSumary& GetPakFileSumary() const override;

protected:
	virtual void Reset() override;
	TSharedPtr<FIoStoreReader> CreateIoStoreReader(const FString& InPath);

	bool InitializeGlobalReader(const FString& InPakPath);
	bool InitializeReaders(const TArray<FString>& InPaks);
	bool PreLoadIoStore(const FString& InTocPath, TMap<FGuid, FAES::FAESKey>& OutKeys);

protected:
	TSharedPtr<FIoStoreReader> GlobalIoStoreReader;
	TArray<FNameEntryId> GlobalNameMap;

	struct FContainerInfo
	{
		FIoContainerId Id;
		FGuid EncryptionKeyGuid;
		bool bCompressed;
		bool bSigned;
		bool bEncrypted;
		bool bIndexed;

		FPakFileSumary Summary;
		TSharedPtr<FIoStoreReader> Reader;
	};

	struct FStorePackageInfo
	{
		FName PackageName;
		FPackageId PackageId;
		int32 ContainerIndex;
		FIoChunkId ChunkId;
		EIoChunkType ChunkType;
		FIoStoreTocChunkInfo ChunkInfo;
	};

	TArray<FContainerInfo> StoreContainers;
	TArray<FStorePackageInfo> PackageInfos;
};

#endif // ENABLE_IO_STORE_ANALYZER
