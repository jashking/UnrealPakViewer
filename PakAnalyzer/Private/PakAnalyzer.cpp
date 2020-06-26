// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PakAnalyzer.h"

#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"
#include "IPlatformFilePak.h"

#include "LogDefines.h"

FPakAnalyzer::FPakAnalyzer()
{

}

FPakAnalyzer::~FPakAnalyzer()
{
	Reset();
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

	Reset();

	PakFile = MakeShared<FPakFile>(*InPakPath, false);
	if (!PakFile.IsValid())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Create PakFile failed! Path: %s."), *InPakPath);

		return false;
	}

	if (!PakFile->IsValid())
	{
		// TODO: Check encryption key
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load pak file failed! Unable to open pak file! Path: %s."), *InPakPath);

		return false;
	}

	LoadGuid = FGuid::NewGuid();

	TArray<FPakFile::FFileIterator> Records;
	for (FPakFile::FFileIterator It(*PakFile, true); It; ++It)
	{
		Records.Add(It);
	}

	struct FOffsetSort
	{
		FORCEINLINE bool operator()(const FPakFile::FFileIterator& A, const FPakFile::FFileIterator& B) const
		{
			return A.Info().Offset < B.Info().Offset;
		}
	};
	Records.Sort(FOffsetSort());

	for (auto It : Records)
	{
		FPakFileEntryPtr PakFileEntry = MakeShared<FPakFileEntry>();

		PakFileEntry->PakEntry = &It.Info();
		PakFileEntry->Filename = FPaths::GetCleanFilename(It.Filename());
		PakFileEntry->Path = It.Filename();

		Files.Add(PakFileEntry);
	}

	return true;
}

int32 FPakAnalyzer::GetFileCount() const
{
	return Files.Num();
}

const TArray<FPakFileEntryPtr >& FPakAnalyzer::GetFiles() const
{
	return Files;
}

FString FPakAnalyzer::GetLastLoadGuid() const
{
	return LoadGuid.ToString();
}

bool FPakAnalyzer::IsLoadDirty(const FString& InGuid) const
{
	return !InGuid.Equals(LoadGuid.ToString(), ESearchCase::IgnoreCase);
}

void FPakAnalyzer::Reset()
{
	Files.Empty();
	PakFile.Reset();
	LoadGuid.Invalidate();
}
