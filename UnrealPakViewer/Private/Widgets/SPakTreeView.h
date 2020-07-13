#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "PakFileEntry.h"

/** Implements the Pak Info window. */
class SPakTreeView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SPakTreeView();

	/** Virtual destructor. */
	virtual ~SPakTreeView();

	SLATE_BEGIN_ARGS(SPakTreeView) {}
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

protected:
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Tree View - Table Row

	/** Called by STreeView to generate a table row for the specified item. */
	TSharedRef<ITableRow> OnGenerateTreeRow(FPakTreeEntryPtr TreeNode, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetTreeNodeChildren(FPakTreeEntryPtr InParent, TArray<FPakTreeEntryPtr>& OutChildren);

protected:
	TSharedPtr<STreeView<FPakTreeEntryPtr>> TreeView;

	/** External scrollbar used to synchronize tree view position. */
	TSharedPtr<class SScrollBar> ExternalScrollbar;

	/** The root node(s) of the tree. */
	TArray<FPakTreeEntryPtr> TreeNodes;

	FString LastLoadGuid;
};
