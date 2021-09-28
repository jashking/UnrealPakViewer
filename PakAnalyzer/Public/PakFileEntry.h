#pragma once

#include "CoreMinimal.h"
#include "IPlatformFilePak.h"
#include "Misc/AES.h"
#include "UObject/ObjectResource.h"
#include "UObject/PackageFileSummary.h"

typedef TSharedPtr<struct FPakClassEntry> FPakClassEntryPtr;
typedef TSharedPtr<FName> FNamePtrType;
typedef TSharedPtr<struct FObjectExportEx> FObjectExportPtrType;
typedef TSharedPtr<struct FObjectImportEx> FObjectImportPtrType;
typedef TSharedPtr<struct FAssetSummary> FAssetSummaryPtr;
typedef TSharedPtr<struct FPakFileEntry> FPakFileEntryPtr;
typedef TSharedPtr<struct FPakTreeEntry> FPakTreeEntryPtr;
typedef TSharedPtr<struct FPackageInfo> FPackageInfoPtr;
typedef TSharedPtr<struct FPakFileSumary> FPakFileSumaryPtr;

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

struct FObjectExportEx
{
	FName ObjectName;
	uint64 SerialSize = 0;
	uint64 SerialOffset = 0;
	bool bIsAsset = false;
	bool bNotForClient = false;
	bool bNotForServer = false;
	int32 Index = 0;
	FName ObjectPath;
	FName ClassName;
	FName TemplateObject;
	FName Super;
	TArray<FPackageInfoPtr> DependencyList;
};

struct FObjectImportEx
{
	int32 Index = 0;
	FName ClassPackage;
	FName ClassName;
	FName ObjectName;
	FName ObjectPath;
};

struct FPackageInfo
{
	FName PackageName;
	FName ExtraInfo;
};

struct FAssetSummary
{
	FPackageFileSummary PackageSummary;
	TArray<FNamePtrType> Names;
	TArray<FObjectExportPtrType> ObjectExports;
	TArray<FObjectImportPtrType> ObjectImports;
	TArray<FPackageInfoPtr> DependencyList; // this asset depends on
	TArray<FPackageInfoPtr> DependentList; // assets depends on this
};

struct FPakFileEntry : TSharedFromThis<FPakFileEntry>
{
	FPakFileEntry(FName InFilename, const FString& InPath)
		: Filename(InFilename)
		, Path(InPath)
	{

	}

	FPakEntry PakEntry;
	FName Filename;
	FString Path;
	FName CompressionMethod;
	FName Class;
	FName PackagePath;
	FAssetSummaryPtr AssetSummary;
	int16 OwnerPakIndex = 0;
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

	FPakTreeEntry(FName InFilename, const FString& InPath, bool bInIsDirectory)
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
	int64 PakFileSize = 0;
	FString CompressionMethods;
	FString DecryptAESKeyStr;
	FAES::FAESKey DecryptAESKey;
	int32 FileCount = 0;
};
