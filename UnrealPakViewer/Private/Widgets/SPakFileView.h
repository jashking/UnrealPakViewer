#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "PakFileEntry.h"

namespace PakFileViewColumns
{
	static const FName IndexColumnName(TEXT("Index"));
	static const FName NameColumnName(TEXT("Name"));
	static const FName PathColumnName(TEXT("Path"));
	static const FName OffsetColumnName(TEXT("Offset"));
	static const FName SizeColumnName(TEXT("Size"));
	static const FName CompressedSizeColumnName(TEXT("CompressedSize"));
	static const FName CompressionMethodColumnName(TEXT("CompressionMethod"));
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
