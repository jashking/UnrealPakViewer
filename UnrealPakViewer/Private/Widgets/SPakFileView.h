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

enum class EFileColumnFlags : uint32
{
	None = 0,

	ShouldBeVisible = (1 << 0),
	CanBeHidden = (1 << 1),
	CanBeFiltered = (1 << 2),
};
ENUM_CLASS_FLAGS(EFileColumnFlags);

/** Implements the Pak Info window. */
class SPakFileView : public SCompoundWidget
{
public:
	typedef TSharedPtr<FPakFileEntry> FPakFileItem;
	typedef TFunction<bool(const FPakFileItem& A, const FPakFileItem& B)> FFileCompareFunc;

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
	class FFileColumn
	{
	public:
		FFileColumn() = delete;
		FFileColumn(int32 InIndex, const FName InId, const FText& InTitleName, const FText& InDescription, float InInitialWidth, const EFileColumnFlags& InFlags, FFileCompareFunc InAscendingCompareDelegate = nullptr, FFileCompareFunc InDescendingCompareDelegate = nullptr)
			: Index(InIndex)
			, Id(InId)
			, TitleName(InTitleName)
			, Description(InDescription)
			, InitialWidth(InInitialWidth)
			, Flags(InFlags)
			, AscendingCompareDelegate(InAscendingCompareDelegate)
			, DescendingCompareDelegate(InDescendingCompareDelegate)
			, bIsVisible(EnumHasAnyFlags(Flags, EFileColumnFlags::ShouldBeVisible))
		{
		}

		int32 GetIndex() const { return Index; }
		const FName& GetId() const { return Id; }
		const FText& GetTitleName() const { return TitleName; }
		const FText& GetDescription() const { return Description; }

		bool IsVisible() const { return bIsVisible; }
		void Show() { bIsVisible = true; }
		void Hide() { bIsVisible = false; }
		void ToggleVisibility() { bIsVisible = !bIsVisible; }
		void SetVisibilityFlag(bool bOnOff) { bIsVisible = bOnOff; }

		float GetInitialWidth() const { return InitialWidth; }

		/** Whether this column should be initially visible. */
		bool ShouldBeVisible() const { return EnumHasAnyFlags(Flags, EFileColumnFlags::ShouldBeVisible); }

		/** Whether this column can be hidden. */
		bool CanBeHidden() const { return EnumHasAnyFlags(Flags, EFileColumnFlags::CanBeHidden); }

		/** Whether this column can be used for filtering displayed results. */
		bool CanBeFiltered() const { return EnumHasAnyFlags(Flags, EFileColumnFlags::CanBeFiltered); }

		/** Whether this column can be used for sort displayed results. */
		bool CanBeSorted() const { return AscendingCompareDelegate && DescendingCompareDelegate; }

		void SetAscendingCompareDelegate(FFileCompareFunc InCompareDelegate) { AscendingCompareDelegate = InCompareDelegate; }
		void SetDescendingCompareDelegate(FFileCompareFunc InCompareDelegate) { DescendingCompareDelegate = InCompareDelegate; }

		FFileCompareFunc GetAscendingCompareDelegate() const { return AscendingCompareDelegate; }
		FFileCompareFunc GetDescendingCompareDelegate() const { return DescendingCompareDelegate; }

protected:
		int32 Index;
		FName Id;
		FText TitleName;
		FText Description;
		float InitialWidth;
		EFileColumnFlags Flags;
		FFileCompareFunc AscendingCompareDelegate;
		FFileCompareFunc DescendingCompareDelegate;

		bool bIsVisible;

	};

protected:
	bool SearchBoxIsEnabled() const;
	void OnSearchBoxTextChanged(const FText& inFilterText);

	/** Generate a new list view row. */
	TSharedRef<ITableRow> OnGenerateFileRow(FPakFileItem InPakFileItem, const TSharedRef<class STableViewBase>& OwnerTable);
	
	/** Generate a right-click context menu. */
	TSharedPtr<SWidget> OnGenerateContextMenu();
	void OnBuildSortByMenu(FMenuBuilder& MenuBuilder);
	void OnBuildCopyColumnMenu(FMenuBuilder& MenuBuilder);
	void OnBuildViewColumnMenu(FMenuBuilder& MenuBuilder);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// File List View - Columns
	void InitializeAndShowHeaderColumns();
	FFileColumn* FindCoulum(const FName ColumnId);

	EColumnSortMode::Type GetSortModeForColumn(const FName ColumnId) const;
	void OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode);
	void SortByColumn(const FName& ColumnId, const EColumnSortMode::Type SortMode);

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
	bool HasFileSelected() const;
	void OnCopyAllColumnsExecute();
	void OnCopyColumnExecute(const FName ColumnId);

	void OnJumpToTreeViewExecute();

protected:
	/** External scrollbar used to synchronize file view position. */
	TSharedPtr<class SScrollBar> ExternalScrollbar;

	/** The search box widget used to filter items displayed in the file view. */
	TSharedPtr<class SSearchBox> SearchBox;

	/** The list view widget. */
	TSharedPtr<SListView<FPakFileItem>> FileListView;

	/** Holds the list view header row widget which display all columns in the list view. */
	TSharedPtr<class SHeaderRow> FileListHeaderRow;

	/** List of files to show in list view (i.e. filtered). */
	TArray<FPakFileItem> FileCache;

	/** Manage show, hide and sort. */
	TMap<FName, FFileColumn> FileColumns;

	FName CurrentSortedColumn = PakFileViewColumns::OffsetColumnName;
	EColumnSortMode::Type CurrentSortMode = EColumnSortMode::Ascending;
};
