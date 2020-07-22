#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "PakFileEntry.h"
#include "ViewModels/ClassColumn.h"

/** Implements the Pak Info window. */
class SPakClassView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SPakClassView();

	/** Virtual destructor. */
	virtual ~SPakClassView();

	SLATE_BEGIN_ARGS(SPakClassView) {}
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

	void Reload(FPakTreeEntryPtr InFolder);

protected:
	/** Generate a new list view row. */
	TSharedRef<ITableRow> OnGenerateClassRow(FPakClassEntryPtr InPakClassItem, const TSharedRef<class STableViewBase>& OwnerTable);
	
	void InitializeAndShowHeaderColumns();
	void ShowColumn(const FName ColumnId);
	FClassColumn* FindCoulum(const FName ColumnId);

	EColumnSortMode::Type GetSortModeForColumn(const FName ColumnId) const;
	void OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode);

	void MarkDirty(bool bInIsDirty);

	void Sort();

protected:
	/** External scrollbar used to synchronize file view position. */
	TSharedPtr<class SScrollBar> ExternalScrollbar;

	/** The list view widget. */
	TSharedPtr<SListView<FPakClassEntryPtr>> ClassListView;

	/** Holds the list view header row widget which display all columns in the list view. */
	TSharedPtr<class SHeaderRow> ClassListHeaderRow;

	/** Manage show, hide and sort. */
	TMap<FName, FClassColumn> ClassColumns;

	/** List of classes to show in list view (i.e. filtered). */
	TArray<FPakClassEntryPtr> ClassCache;

	bool bIsDirty = false;
	FName CurrentSortedColumn = FClassColumn::CompressedSizeColumnName;
	EColumnSortMode::Type CurrentSortMode = EColumnSortMode::Descending;

	FString LastLoadGuid;
};
