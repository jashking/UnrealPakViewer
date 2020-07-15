// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PakAnalyzer.h"

#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "IPlatformFilePak.h"

#include "LogDefines.h"

FPakAnalyzer::FPakAnalyzer()
{
	Reset();
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

	// Generate unique id
	LoadGuid = FGuid::NewGuid();

	// Save pak sumary
	PakFileSumary.MountPoint = PakFile->GetMountPoint();
	PakFileSumary.PakInfo = &PakFile->GetInfo();
	PakFileSumary.PakFilePath = InPakPath;
	PakFileSumary.PakFileSize = PakFile->TotalSize();

	// Make tree root
	TreeRoot = MakeShared<FPakTreeEntry>(FPaths::GetCleanFilename(InPakPath), PakFileSumary.MountPoint, nullptr, true);

	// Iterate Files
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
			InsertTreeNode(It.Filename(), &It.Info(), false);
		}

		RefreshTreeNode(TreeRoot);
	}

	return true;
}

int32 FPakAnalyzer::GetFileCount() const
{
	return TreeRoot.IsValid() ? TreeRoot->FileCount : 0;
}

void FPakAnalyzer::GetFiles(const FString& InFilterText, TArray<FPakFileEntryPtr>& OutFiles) const
{
	FScopeLock Lock(const_cast<FCriticalSection*>(&CriticalSection));

	RetriveFiles(TreeRoot, InFilterText, OutFiles);
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

FPakTreeEntryPtr FPakAnalyzer::GetPakTreeRootNode() const
{
	return TreeRoot;
}

void FPakAnalyzer::Reset()
{
	PakFile.Reset();
	LoadGuid.Invalidate();

	PakFileSumary.MountPoint = TEXT("");
	PakFileSumary.PakInfo = nullptr;
	PakFileSumary.PakFilePath = TEXT("");
	PakFileSumary.PakFileSize = 0;

	TreeRoot.Reset();
}

FPakTreeEntryPtr FPakAnalyzer::InsertTreeNode(const FString& InFullPath, const FPakEntry* InEntry, bool bIsDirectory)
{
	const FString BasePath = FPaths::GetPath(InFullPath);
	const FString LeafName = FPaths::GetPathLeaf(InFullPath);

	FPakTreeEntryPtr Parent = BasePath.IsEmpty() ? TreeRoot : InsertTreeNode(BasePath, nullptr, true);

	for (FPakTreeEntryPtr Child : Parent->Children)
	{
		if (Child->Filename.Equals(LeafName, ESearchCase::IgnoreCase))
		{
			return Child;
		}
	}

	FPakTreeEntryPtr NewChild = MakeShared<FPakTreeEntry>(LeafName, InFullPath, InEntry, bIsDirectory);
	Parent->Children.Add(NewChild);

	return NewChild;
}

void FPakAnalyzer::RefreshTreeNode(FPakTreeEntryPtr InRoot)
{
	for (FPakTreeEntryPtr Child : InRoot->Children)
	{
		if (Child->bIsDirectory)
		{
			RefreshTreeNode(Child);
		}
		else
		{
			Child->FileCount = 1;
			Child->Size = Child->PakEntry->UncompressedSize;
			Child->CompressedSize = Child->PakEntry->Size;
		}

		InRoot->FileCount += Child->FileCount;
		InRoot->Size += Child->Size;
		InRoot->CompressedSize += Child->CompressedSize;
	}

	InRoot->Children.Sort([](const FPakTreeEntryPtr& A, const FPakTreeEntryPtr& B) -> bool
		{
			if (A->bIsDirectory == B->bIsDirectory)
			{
				return A->Filename < B->Filename;
			}

			return (int32)A->bIsDirectory > (int32)B->bIsDirectory;
		});
}

void FPakAnalyzer::RetriveFiles(FPakTreeEntryPtr InRoot, const FString& InFilterText, TArray<FPakFileEntryPtr>& OutFiles) const
{
	for (FPakTreeEntryPtr Child : InRoot->Children)
	{
		if (Child->bIsDirectory)
		{
			RetriveFiles(Child, InFilterText, OutFiles);
		}
		else
		{
			if (InFilterText.IsEmpty() || Child->Filename.Contains(InFilterText) || Child->Path.Contains(InFilterText))
			{
				FPakFileEntryPtr FileEntryPtr = MakeShared<FPakFileEntry>(Child->Filename, Child->Path, Child->PakEntry);
				OutFiles.Add(FileEntryPtr);
			}
		}
	}
}
