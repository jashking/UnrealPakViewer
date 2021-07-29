#pragma once

#include "CoreMinimal.h"
#include "IPlatformFilePak.h"
#include "UObject/ObjectResource.h"
#include "UObject/PackageFileSummary.h"

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

typedef TSharedPtr<FName> FNamePtrType;
typedef TSharedPtr<FObjectExport> FObjectExportPtrType;
typedef TSharedPtr<FObjectImport> FObjectImportPtrType;
typedef TSharedPtr<FPackageIndex> FPackageIndexPtrType;

struct FAssetSummary
{
	FPackageFileSummary PackageSummary;
	TArray<FNamePtrType> Names;
	TArray<FObjectExportPtrType> ObjectExports;
	TArray<FObjectImportPtrType> ObjectImports;
	TArray<FPackageIndexPtrType> PreloadDependency;
};

typedef TSharedPtr<FAssetSummary> FAssetSummaryPtr;

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
	FAssetSummaryPtr AssetSummary;
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
