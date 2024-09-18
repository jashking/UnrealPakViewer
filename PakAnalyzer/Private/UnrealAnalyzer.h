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
#include "IoStoreAnalyzer.h"
#include "PakAnalyzer.h"

struct FPakEntry;

class FUnrealAnalyzer : public FBaseAnalyzer, public TSharedFromThis<FUnrealAnalyzer>
{
public:
	FUnrealAnalyzer();
	virtual ~FUnrealAnalyzer();

	virtual bool LoadPakFiles(const TArray<FString>& InPakPaths, const TArray<FString>& InDefaultAESKeys, int32 ContainerStartIndex = 0) override;
	virtual void ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles) override;
	virtual void CancelExtract() override;
	virtual void SetExtractThreadCount(int32 InThreadCount) override;
	virtual void Reset() override;

protected:
	TSharedPtr<FPakAnalyzer> PakAnalyzer;
	TSharedPtr<FIoStoreAnalyzer> IoStoreAnalyzer;
};
