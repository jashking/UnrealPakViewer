#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "PakFileEntry.h"

#include "Runtime/Launch/Resources/Version.h"

#define DECLARE_GET_MEMBER_FUNCTION(MemberName) FORCEINLINE FText Get##MemberName() const

class SHeaderRow;

/** Implements the Pak Info window. */
class SAssetSummaryView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SAssetSummaryView();

	/** Virtual destructor. */
	virtual ~SAssetSummaryView();

	SLATE_BEGIN_ARGS(SAssetSummaryView) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);

	void SetViewingPackage(FPakFileEntryPtr InPackage);

protected:
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
#else
	DECLARE_GET_MEMBER_FUNCTION(Guid);
#endif
	DECLARE_GET_MEMBER_FUNCTION(IsUnversioned);
	DECLARE_GET_MEMBER_FUNCTION(FileVersionUE4);
	DECLARE_GET_MEMBER_FUNCTION(FileVersionUE5);
	DECLARE_GET_MEMBER_FUNCTION(FileVersionLicenseeUE);
	DECLARE_GET_MEMBER_FUNCTION(TotalHeaderSize);
	DECLARE_GET_MEMBER_FUNCTION(NameCount);
	DECLARE_GET_MEMBER_FUNCTION(NameOffset);
	DECLARE_GET_MEMBER_FUNCTION(ExportCount);
	DECLARE_GET_MEMBER_FUNCTION(ExportOffset);
	DECLARE_GET_MEMBER_FUNCTION(ExportSize);
	DECLARE_GET_MEMBER_FUNCTION(ExportSizeTooltip);
	DECLARE_GET_MEMBER_FUNCTION(ImportCount);
	DECLARE_GET_MEMBER_FUNCTION(ImportOffset);
	DECLARE_GET_MEMBER_FUNCTION(PackageFlags);
	DECLARE_GET_MEMBER_FUNCTION(GatherableTextDataCount);
	DECLARE_GET_MEMBER_FUNCTION(GatherableTextDataOffset);
	DECLARE_GET_MEMBER_FUNCTION(DependsOffset);
	DECLARE_GET_MEMBER_FUNCTION(SoftPackageReferencesCount);
	DECLARE_GET_MEMBER_FUNCTION(SoftPackageReferencesOffset);
	DECLARE_GET_MEMBER_FUNCTION(SearchableNamesOffset);
	DECLARE_GET_MEMBER_FUNCTION(ThumbnailTableOffset);
	DECLARE_GET_MEMBER_FUNCTION(AssetRegistryDataOffset);
	DECLARE_GET_MEMBER_FUNCTION(BulkDataStartOffset);
	DECLARE_GET_MEMBER_FUNCTION(WorldTileInfoDataOffset);
	DECLARE_GET_MEMBER_FUNCTION(PreloadDependencyCount);
	DECLARE_GET_MEMBER_FUNCTION(PreloadDependencyOffset);
	DECLARE_GET_MEMBER_FUNCTION(DependencyCount);
	DECLARE_GET_MEMBER_FUNCTION(DependentCount);

	TSharedRef<ITableRow> OnGenerateNameRow(FNamePtrType InName, const TSharedRef<class STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateImportObjectRow(FObjectImportPtrType InObject, const TSharedRef<class STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateExportObjectRow(FObjectExportPtrType InObject, const TSharedRef<class STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateDependsRow(FPackageInfoPtr InDepends, const TSharedRef<class STableViewBase>& OwnerTable);

	void InsertColumn(TSharedPtr<SHeaderRow> InHeader, FName InId, const FString& InCloumnName = TEXT(""));
	void InsertSortableColumn(TSharedPtr<SHeaderRow> InHeader, FName InId, const FString& InCloumnName = TEXT(""));
	void OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode);
	EColumnSortMode::Type GetSortModeForColumn(const FName ColumnId) const;
	void OnSortExportObjects();

protected:
	FPakFileEntryPtr ViewingPackage;

	TSharedPtr<SListView<FNamePtrType>> NamesListView;
	TArray<FNamePtrType> PackageNames;

	TSharedPtr<SHeaderRow> ImportObjectHeaderRow;
	TSharedPtr<SListView<FObjectImportPtrType>> ImportObjectListView;
	TArray<FObjectImportPtrType> ImportObjects;

	TSharedPtr<SHeaderRow> ExportObjectHeaderRow;
	TSharedPtr<SListView<FObjectExportPtrType>> ExportObjectListView;
	TArray<FObjectExportPtrType> ExportObjects;
	int64 TotalExportSize;
	
	FName LastSortColumn = "SerialOffset";
	EColumnSortMode::Type LastSortMode = EColumnSortMode::Ascending;

	//TSharedPtr<SListView<FPackageIndexPtrType>> PreloadDependencyListView;
	//TArray<FPackageIndexPtrType> PreloadDependency;

	TSharedPtr<SListView<FPackageInfoPtr>> DependencyListView;
	TSharedPtr<SListView<FPackageInfoPtr>> DependentListView;
	TArray<FPackageInfoPtr> DependencyList;
	TArray<FPackageInfoPtr> DependentList;
};
