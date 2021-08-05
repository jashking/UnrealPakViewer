#pragma once

#include "CoreMinimal.h"
#include "IPlatformFilePak.h"
#include "UObject/ObjectResource.h"
#include "UObject/PackageFileSummary.h"

typedef TSharedPtr<struct FPakClassEntry> FPakClassEntryPtr;
typedef TSharedPtr<FName> FNamePtrType;
typedef TSharedPtr<struct FObjectExportEx> FObjectExportPtrType;
typedef TSharedPtr<struct FObjectImportEx> FObjectImportPtrType;
typedef TSharedPtr<class FPackageIndex> FPackageIndexPtrType;
typedef TSharedPtr<struct FAssetSummary> FAssetSummaryPtr;
typedef TSharedPtr<struct FPakFileEntry> FPakFileEntryPtr;
typedef TSharedPtr<struct FPakTreeEntry> FPakTreeEntryPtr;
typedef TSharedPtr<struct FPackageInfo> FPackageInfoPtr;

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

struct FObjectExportEx : public FObjectExport
{
	int32 Index = 0;
	FString ObjectPath;
	FString ClassName;
	FString TemplateObject;
	FString Super;
};

struct FObjectImportEx : public FObjectImport
{
	int32 Index = 0;
	FString ObjectPath;
};

struct FPackageInfo
{
	FString PackageName;
};

struct FAssetSummary
{
	FPackageFileSummary PackageSummary;
	TArray<FNamePtrType> Names;
	TArray<FObjectExportPtrType> ObjectExports;
	TArray<FObjectImportPtrType> ObjectImports;
	TArray<FPackageIndexPtrType> PreloadDependency;
	TArray<FPackageInfoPtr> DependencyList; // this asset depends on
	TArray<FPackageInfoPtr> DependentList; // assets depends on this
};

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
	FString PackagePath;
	FAssetSummaryPtr AssetSummary;
};

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

struct FPakFileSumary
{
	FPakInfo PakInfo;
	FString MountPoint;
	FString PakFilePath;
	int64 PakFileSize;
	FString CompressionMethods;
	FString AssetRegistryPath;
};
