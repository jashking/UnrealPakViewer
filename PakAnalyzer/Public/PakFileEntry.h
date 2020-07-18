#pragma once

#include "CoreMinimal.h"
#include "IPlatformFilePak.h"

struct FPakFileEntry : TSharedFromThis<FPakFileEntry>
{
	FPakFileEntry(const FString& InFilename, const FString& InPath)
		: Filename(*InFilename)
		, Path(InPath)
	{

	}

	FPakEntry PakEntry;
	FName Filename;
	FString Path;
};

typedef TSharedPtr<FPakFileEntry> FPakFileEntryPtr;

struct FPakTreeEntry : public FPakFileEntry
{
	int32 FileCount;
	int64 Size;
	int64 CompressedSize;
	float CompressedSizePercentOfTotal;
	float CompressedSizePercentOfParent;

	bool bIsDirectory;
	TMap<FName, TSharedPtr<FPakTreeEntry>> ChildrenMap;

	FPakTreeEntry(const FString& InFilename, const FString& InPath, bool bInIsDirectory)
		: FPakFileEntry(InFilename, InPath)
		, FileCount(0)
		, Size(0)
		, CompressedSize(0)
		, CompressedSizePercentOfTotal(1.f)
		, CompressedSizePercentOfParent(1.f)
		, bIsDirectory(bInIsDirectory)
	{

	}
};

typedef TSharedPtr<FPakTreeEntry> FPakTreeEntryPtr;

struct FPakFileSumary
{
	const FPakInfo* PakInfo = nullptr;
	FString MountPoint;
	FString PakFilePath;
	int64 PakFileSize;
};
