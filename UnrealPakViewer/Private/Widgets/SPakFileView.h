#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "PakFileEntry.h"

namespace PakFileViewColumns
{
	static const FName NameColumnName(TEXT("Name"));
	static const FName PathColumnName(TEXT("Path"));
	static const FName OffsetColumnName(TEXT("Offset"));
	static const FName SizeColumnName(TEXT("Size"));
	static const FName CompressedSizeColumnName(TEXT("CompressedSize"));
	static const FName CompressionBlockCountColumnName(TEXT("CompressionBlockCount"));
	static const FName CompressionBlockSizeColumnName(TEXT("CompressionBlockSize"));
	static const FName SHA1ColumnName(TEXT("SHA1"));
	static const FName IsEncryptedColumnName(TEXT("IsEncrypted"));
}

/** Implements the Pak Info window. */
class SPakFileView : public SCompoundWidget
{
public:
	typedef TSharedPtr<FPakFileEntry> FPakFileItem;

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

protected:
	bool SearchBoxIsEnabled() const;
	void OnSearchBoxTextChanged(const FText& inFilterText);

	/** Generate a new list view row. */
	TSharedRef<ITableRow> OnGenerateFileRow(FPakFileItem InPakFileItem, const TSharedRef<class STableViewBase>& OwnerTable);

protected:
	/** External scrollbar used to synchronize file view position. */
	TSharedPtr<class SScrollBar> ExternalScrollbar;

	/** The search box widget used to filter items displayed in the file view. */
	TSharedPtr<class SSearchBox> SearchBox;

	/** The list view widget. */
	TSharedPtr<SListView<FPakFileItem>> FileListView;

	/** List of files to show in list view (i.e. filtered). */
	TArray<FPakFileItem> FileCache;
};
