#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

#include "PakFileEntry.h"

class SKeyValueRow;
class SVerticalBox;

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

	FORCEINLINE void SetDelayHighlightItem(const FString& InPath) { DelayHighlightItem = InPath; }

protected:
	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Tree View - Table Row

	/** Called by STreeView to generate a table row for the specified item. */
	TSharedRef<ITableRow> OnGenerateTreeRow(FPakTreeEntryPtr TreeNode, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetTreeNodeChildren(FPakTreeEntryPtr InParent, TArray<FPakTreeEntryPtr>& OutChildren);
	
	/** Called by STreeView when selection has changed. */
	void OnSelectionChanged(FPakTreeEntryPtr SelectedItem, ESelectInfo::Type SelectInfo);

	void ExpandTreeItem(const FString& InPath);

	// Detail View
	FORCEINLINE FText GetSelectionName() const;
	FORCEINLINE FText GetSelectionPath() const;
	FORCEINLINE FText GetSelectionOffset() const;
	FORCEINLINE FText GetSelectionSize() const;
	FORCEINLINE FText GetSelectionSizeToolTip() const;
	FORCEINLINE FText GetSelectionCompressedSize() const;
	FORCEINLINE FText GetSelectionCompressedSizeToolTip() const;
	FORCEINLINE FText GetSelectionCompressedSizePercentOfTotal() const;
	FORCEINLINE FText GetSelectionCompressedSizePercentOfParent() const;
	FORCEINLINE FText GetSelectionCompressionBlockCount() const;
	FORCEINLINE FText GetSelectionCompressionBlockSize() const;
	FORCEINLINE FText GetSelectionCompressionBlockSizeToolTip() const;
	FORCEINLINE FText GetCompressionMethod() const;
	FORCEINLINE FText GetSelectionSHA1() const;
	FORCEINLINE FText GetSelectionIsEncrypted() const;
	FORCEINLINE FText GetSelectionFileCount() const;
	FORCEINLINE const FSlateBrush* GetFolderImage(FPakTreeEntryPtr InTreeNode) const;
	FORCEINLINE FText GetSelectionClass() const;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// Tree View - Context Menu
	TSharedPtr<SWidget> OnGenerateContextMenu();

	void OnExtractExecute();
	void OnJumpToFileViewExecute();
	bool HasSelection() const;
	bool HasFileSelection() const;
	void OnExportToJson();
	void OnExportToCsv();

	void RetriveFiles(FPakTreeEntryPtr InRoot, TArray<FPakFileEntryPtr>& OutFiles);

	void OnLoadAssetReigstryFinished();

protected:
	TSharedPtr<STreeView<FPakTreeEntryPtr>> TreeView;
	FPakTreeEntryPtr CurrentSelectedItem;

	/** The root node(s) of the tree. */
	TArray<FPakTreeEntryPtr> TreeNodes;

	FString LastLoadGuid;

	TSharedPtr<SVerticalBox> KeyValueBox;
	TSharedPtr<SKeyValueRow> OffsetRow;
	TSharedPtr<SKeyValueRow> CompressionBlockCountRow;
	TSharedPtr<SKeyValueRow> CompressionBlockSizeRow;
	TSharedPtr<SKeyValueRow> CompressionMethodRow;
	TSharedPtr<SKeyValueRow> SHA1Row;
	TSharedPtr<SKeyValueRow> IsEncryptedRow;
	TSharedPtr<SKeyValueRow> FileCountRow;
	TSharedPtr<class SPakClassView> ClassView;
	TSharedPtr<SKeyValueRow> ClassRow;

	FString DelayHighlightItem;

	bool bIsDirty = false;
};
