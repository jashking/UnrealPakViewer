// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PakAnalyzer.h"

#include "HAL/PlatformFile.h"
#include "IPlatformFilePak.h"

#include "LogDefines.h"

TSharedPtr<FPakAnalyzer> FPakAnalyzer::Instance = nullptr;

FPakAnalyzer::FPakAnalyzer()
{

}

FPakAnalyzer::~FPakAnalyzer()
{

}

TSharedPtr<FPakAnalyzer> FPakAnalyzer::Get()
{
	return FPakAnalyzer::Instance;
}

void FPakAnalyzer::Initialize()
{
	if (!FPakAnalyzer::Instance.IsValid())
	{
		FPakAnalyzer::Instance = MakeShared<FPakAnalyzer>();
	}
}

void FPakAnalyzer::Shutdown()
{
	FPakAnalyzer::Instance.Reset();
}

bool FPakAnalyzer::LoadPakFile(const FString& InPakPath)
{
	if (InPakPath.IsEmpty())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Pak path is empty!"));
		return false;
	}

	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	if (!PlatformFile.FileExists(*InPakPath))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Pak file not exists! Path: %s."), *InPakPath);
		return false;
	}

	PakFile = MakeShared<FPakFile>(*InPakPath, false);
	if (!PakFile.IsValid())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Create PakFile failed! Path: %s."), *InPakPath);

		return false;
	}

	return true;
}
