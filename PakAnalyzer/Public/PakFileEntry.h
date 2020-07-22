#pragma once

#include "CoreMinimal.h"
#include "IPlatformFilePak.h"

struct FPakClassEntry
{
	FPakClassEntry(FName InClassName, int64 InSize, int64 InCompressedSize, int32 InFileCount)
		: Class(InClassName)
		, Size(InSize)
		, CompressedSize(InCompressedSize)
		, FileCount(InFileCount)
		, PercentOfTotal(1.f)
		, PercentOfParent(1.f)
	{

	}

	FName Class;
	int64 Size;
	int64 CompressedSize;
	int32 FileCount;
	float PercentOfTotal;
	float PercentOfParent;
};

typedef TSharedPtr<FPakClassEntry> FPakClassEntryPtr;

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
	FName CompressionMethod;
	FName Class;
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
	TMap<FName, FPakClassEntryPtr> FileClassMap;

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
	FPakInfo PakInfo;
	FString MountPoint;
	FString PakFilePath;
	int64 PakFileSize;
	FString CompressionMethods;
	FString AssetRegistryPath;
};
