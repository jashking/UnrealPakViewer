#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "ViewModels/FileColumn.h"
#include "PakFileEntry.h"

/** Implements the Pak Info window. */
class SPakFileView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SPakFileView();

	/** Virtual destructor. */
	virtual ~SPakFileView();

	SLATE_BEGIN_ARGS(SPakFileView) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);

	/**
	 * Ticks this widget. Override in derived classes, but always call the parent implementation.
	 *
	 * @param AllottedGeometry - The space allotted for this widget
	 * @param InCurrentTime - Current absolute real time
	 * @param InDeltaTime - Real time passed since last tick
	 */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	const FFileColumn* FindCoulum(const FName ColumnId) const;
	FFileColumn* FindCoulum(const FName ColumnId);

	FText GetSearchText() const;

	FORCEINLINE void SetDelayHighlightItem(const FString& InPath) { DelayHighlightItem = InPath; }

protected:
	bool SearchBoxIsEnabled() const;
	void OnSearchBoxTextChanged(const FText& InFilterText);

	/** Generate a new list view row. */
	TSharedRef<ITableRow> OnGenerateFileRow(FPakFileEntryPtr InPakFileItem, const TSharedRef<class STableViewBase>& OwnerTable);
	
	/** Generate a right-click context menu. */
	TSharedPtr<SWidget> OnGenerateContextMenu();
	void OnBuildSortByMenu(FMenuBuilder& MenuBuilder);
	void OnBuildCopyColumnMenu(FMenuBuilder& MenuBuilder);
	void OnBuildViewColumnMenu(FMenuBuilder& MenuBuilder);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// File List View - Class Filter
	TSharedRef<SWidget> OnBuildClassFilterMenu();

	void OnShowAllClassesExecute();
	bool IsShowAllClassesChecked() const;
	void OnToggleClassesExecute(FName InClassName);
	bool IsClassesFilterChecked(FName InClassName) const;
	void FillClassesFilter();

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// File List View - Columns
	void InitializeAndShowHeaderColumns();

	EColumnSortMode::Type GetSortModeForColumn(const FName ColumnId) const;
	void OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode);

	// ShowColumn
	bool CanShowColumn(const FName ColumnId) const;
	void ShowColumn(const FName ColumnId);

	// HideColumn
	bool CanHideColumn(const FName ColumnId);
	void HideColumn(const FName ColumnId);

	// ToggleColumnVisibility
	bool IsColumnVisible(const FName ColumnId);
	bool CanToggleColumnVisibility(const FName ColumnId);
	void ToggleColumnVisibility(const FName ColumnId);

	// ShowAllColumns (ContextMenu)
	bool OnShowAllColumnsCanExecute() const;
	void OnShowAllColumnsExecute();

	// CopyAllColumns (ContextMenu)
	bool HasOneFileSelected() const;
	bool HasFileSelected() const;
	void OnCopyAllColumnsExecute();
	void OnCopyColumnExecute(const FName ColumnId);

	void OnJumpToTreeViewExecute();

	void MarkDirty(bool bInIsDirty);
	void OnSortAndFilterFinihed(const FName InSortedColumn, EColumnSortMode::Type InSortMode, const FString& InSearchText, const FString& InLoadGuid);

	FText GetFileCount() const;

	// Export
	bool IsFileListEmpty() const;
	void OnExportToJson();
	void OnExportToCsv();
	void OnExtract();

	void ScrollToItem(const FString& InPath);

	void OnLoadAssetReigstryFinished();

	void FillFilesSummary();
	bool GetSelectedItems(TArray<FPakFileEntryPtr>& OutSelectedItems) const;

protected:
	/** External scrollbar used to synchronize file view position. */
	TSharedPtr<class SScrollBar> ExternalScrollbar;

	/** The search box widget used to filter items displayed in the file view. */
	TSharedPtr<class SSearchBox> SearchBox;

	/** The list view widget. */
	TSharedPtr<SListView<FPakFileEntryPtr>> FileListView;

	/** Holds the list view header row widget which display all columns in the list view. */
	TSharedPtr<class SHeaderRow> FileListHeaderRow;

	/** List of files to show in list view (i.e. filtered). */
	TArray<FPakFileEntryPtr> FileCache;

	/** Manage show, hide and sort. */
	TMap<FName, FFileColumn> FileColumns;

	/** The async task to sort and filter file on a worker thread */
	TUniquePtr<FAsyncTask<class FFileSortAndFilterTask>> SortAndFilterTask;
	FFileSortAndFilterTask* InnderTask;

	FName CurrentSortedColumn = FFileColumn::OffsetColumnName;
	EColumnSortMode::Type CurrentSortMode = EColumnSortMode::Ascending;
	FString CurrentSearchText;

	bool bIsDirty = false;

	FString LastLoadGuid;

	FString DelayHighlightItem;

	TMap<FName, bool> ClassFilterMap;

	FPakFileEntryPtr FilesSummary;
};
