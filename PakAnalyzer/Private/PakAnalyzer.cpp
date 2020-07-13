// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PakAnalyzer.h"

#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
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

	{
		FScopeLock Lock(&CriticalSection);

		for (auto It : Records)
		{
			FPakFileEntryPtr PakFileEntry = MakeShared<FPakFileEntry>();

			PakFileEntry->PakEntry = &It.Info();
			PakFileEntry->Filename = FPaths::GetCleanFilename(It.Filename());
			PakFileEntry->Path = It.Filename();

			Files.Add(PakFileEntry);
		}
	}

	PakFileSumary.MountPoint = PakFile->GetMountPoint();
	PakFileSumary.PakInfo = &PakFile->GetInfo();

	PakFilePath = InPakPath;

	return true;
}

int32 FPakAnalyzer::GetFileCount() const
{
	FScopeLock Lock(const_cast<FCriticalSection*>(&CriticalSection));

	return Files.Num();
}

const TArray<FPakFileEntryPtr >& FPakAnalyzer::GetFiles() const
{
	FScopeLock Lock(const_cast<FCriticalSection*>(&CriticalSection));

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

const FPakFileSumary& FPakAnalyzer::GetPakFileSumary() const
{
	return PakFileSumary;
}

FString FPakAnalyzer::GetPakFilePath() const
{
	return PakFilePath;
}

bool FPakAnalyzer::GetPakFilesInDirectory(const FString& InDirectory, bool bIncludeFiles, bool bIncludeDirectories, bool bRecursive, TArray<FPakTreeEntryPtr>& OutFiles) const
{
	OutFiles.Empty();

	if (PakFile.IsValid())
	{
		TArray<FString> PakFiles;
		PakFile->FindFilesAtPath(PakFiles, *InDirectory, bIncludeFiles, bIncludeDirectories, bRecursive);
		if (PakFiles.Num() > 0)
		{
			for (const FString& File : PakFiles)
			{
				FPakTreeEntryPtr PakFileEntry = MakeShared<FPakTreeEntry>();
				if (File.EndsWith(TEXT("/")) || File.EndsWith(TEXT("\\")))
				{
					PakFileEntry->Filename = FPaths::GetPathLeaf(File);
					PakFileEntry->bIsDirectory = true;
				}
				else
				{
					PakFileEntry->Filename = FPaths::GetCleanFilename(File);
					PakFileEntry->bIsDirectory = false;
				}

				PakFileEntry->Path = File;

				OutFiles.Add(PakFileEntry);
			}

			OutFiles.Sort([](const FPakTreeEntryPtr& A, const FPakTreeEntryPtr& B) -> bool
				{
					if (A->bIsDirectory == B->bIsDirectory)
					{
						return A->Filename < B->Filename;
					}

					return (int32)A->bIsDirectory > (int32)B->bIsDirectory;
				});

			return true;
		}
	}

	return false;
}

void FPakAnalyzer::Reset()
{
	{
		FScopeLock Lock(&CriticalSection);
		Files.Empty();
	}
	
	PakFile.Reset();
	LoadGuid.Invalidate();

	PakFileSumary.MountPoint = TEXT("");
	PakFileSumary.PakInfo = nullptr;

	PakFilePath = TEXT("");
}
