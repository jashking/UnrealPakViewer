// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UnrealAnalyzer.h"

FUnrealAnalyzer::FUnrealAnalyzer()
{
	IoStoreAnalyzer = MakeShared<FIoStoreAnalyzer>();
	PakAnalyzer = MakeShared<FPakAnalyzer>();
	
	Reset();
}

FUnrealAnalyzer::~FUnrealAnalyzer()
{
	Reset();

	IoStoreAnalyzer.Reset();
	PakAnalyzer.Reset();
}

bool FUnrealAnalyzer::LoadPakFiles(const TArray<FString>& InPakPaths, const TArray<FString>& InDefaultAESKeys, int32 ContainerStartIndex)
{
	bool bResult = true;

	if (PakAnalyzer)
	{
		bResult &= PakAnalyzer->LoadPakFiles(InPakPaths, InDefaultAESKeys);
		PakTreeRoots = PakAnalyzer->GetPakTreeRootNode();
		PakFileSummaries = PakAnalyzer->GetPakFileSumary();
	}
	
	if (IoStoreAnalyzer)
	{
		bResult &= IoStoreAnalyzer->LoadPakFiles(InPakPaths, InDefaultAESKeys, PakAnalyzer ? PakAnalyzer->GetPakFileSumary().Num() : 0);
		PakTreeRoots += IoStoreAnalyzer->GetPakTreeRootNode();
		PakFileSummaries += IoStoreAnalyzer->GetPakFileSumary();
	}

	FPakAnalyzerDelegates::OnPakLoadFinish.Broadcast();
	
	return bResult;
}

void FUnrealAnalyzer::ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles)
{
	if (IoStoreAnalyzer)
	{
		IoStoreAnalyzer->ExtractFiles(InOutputPath, InFiles);
	}

	if (PakAnalyzer)
	{
		PakAnalyzer->ExtractFiles(InOutputPath, InFiles);
	}
}

void FUnrealAnalyzer::CancelExtract()
{
	if (IoStoreAnalyzer)
	{
		IoStoreAnalyzer->CancelExtract();
	}

	if (PakAnalyzer)
	{
		PakAnalyzer->CancelExtract();
	}
}

void FUnrealAnalyzer::SetExtractThreadCount(int32 InThreadCount)
{
	if (IoStoreAnalyzer)
	{
		IoStoreAnalyzer->SetExtractThreadCount(InThreadCount);
	}

	if (PakAnalyzer)
	{
		PakAnalyzer->SetExtractThreadCount(InThreadCount);
	}
}

void FUnrealAnalyzer::Reset()
{
	if (IoStoreAnalyzer)
	{
		IoStoreAnalyzer->Reset();
	}

	if (PakAnalyzer)
	{
		PakAnalyzer->Reset();
	}
}