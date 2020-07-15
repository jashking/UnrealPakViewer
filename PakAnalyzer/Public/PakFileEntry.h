#pragma once

#include "CoreMinimal.h"

struct FPakEntry;
struct FPakInfo;

struct FPakFileEntry : TSharedFromThis<FPakFileEntry>
{
	FPakFileEntry()
		: PakEntry(nullptr)
		, Filename(TEXT(""))
		, Path(TEXT(""))
	{

	}

	FPakFileEntry(const FString& InFilename, const FString& InPath, const FPakEntry* InPakEntry)
		: PakEntry(InPakEntry)
		, Filename(InFilename)
		, Path(InPath)
	{

	}

	const FPakEntry* PakEntry;
	FString Filename;
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
	TArray<TSharedPtr<FPakTreeEntry>> Children;

	FPakTreeEntry(const FString& InFilename, const FString& InPath, const FPakEntry* InPakEntry, bool bInIsDirectory)
		: FPakFileEntry(InFilename, InPath, InPakEntry)
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
