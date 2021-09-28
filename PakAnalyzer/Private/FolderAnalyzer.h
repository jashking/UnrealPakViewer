// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "HAL/CriticalSection.h"
#include "IPlatformFilePak.h"
#include "Misc/AES.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "Serialization/ArrayReader.h"

#include "BaseAnalyzer.h"

struct FPakEntry;

class FFolderAnalyzer : public FBaseAnalyzer, public TSharedFromThis<FFolderAnalyzer>
{
public:
	FFolderAnalyzer();
	virtual ~FFolderAnalyzer();

	virtual bool LoadPakFiles(const TArray<FString>& InPakPaths, const TArray<FString>& InDefaultAESKeys) override;
	virtual void ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles) override;
	virtual void CancelExtract() override;
	virtual void SetExtractThreadCount(int32 InThreadCount) override;

protected:
	void ParseAssetFile(FPakTreeEntryPtr InRoot);
	void InitializeAssetParseWorker();
	void ShutdownAssetParseWorker();
	void OnReadAssetContent(FPakFileEntryPtr InFile, bool& bOutSuccess, TArray<uint8>& OutContent);

protected:
	TSharedPtr<class FAssetParseThreadWorker> AssetParseWorker;
};
