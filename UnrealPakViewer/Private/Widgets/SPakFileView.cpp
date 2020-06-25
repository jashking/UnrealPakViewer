#include "SPakFileView.h"

#include "EditorStyle.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IPlatformFilePak.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

#include "PakAnalyzerModule.h"

#define LOCTEXT_NAMESPACE "SPakFileView"

// TODO: Compression Method, use index and find in array

////////////////////////////////////////////////////////////////////////////////////////////////////
// SPakFileRow
////////////////////////////////////////////////////////////////////////////////////////////////////

class SPakFileRow : public SMultiColumnTableRow<SPakFileView::FPakFileItem>
{
	SLATE_BEGIN_ARGS(SPakFileRow) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, SPakFileView::FPakFileItem InPakFileItem, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		WeakPakFileItem = MoveTemp(InPakFileItem);

		SMultiColumnTableRow<SPakFileView::FPakFileItem>::Construct(FSuperRowType::FArguments(), InOwnerTableView);

		TSharedRef<SWidget> Row = ChildSlot.GetChildAt(0);

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(new FSlateColorBrush(FLinearColor::White))
			.BorderBackgroundColor(FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)))
			[
				Row
			]
		];
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == PakFileViewColumns::NameColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetName).ToolTipText(this, &SPakFileRow::GetName)
				];
		}
		else if (ColumnName == PakFileViewColumns::PathColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetPath).ToolTipText(this, &SPakFileRow::GetPath)
				];
		}
		else if (ColumnName == PakFileViewColumns::OffsetColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetOffset)
				];
		}
		else if (ColumnName == PakFileViewColumns::SizeColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetSize).ToolTipText(this, &SPakFileRow::GetSizeToolTip)
				];
		}
		else if (ColumnName == PakFileViewColumns::CompressedSizeColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetCompressedSize).ToolTipText(this, &SPakFileRow::GetCompressedSizeToolTip)
				];
		}
		else if (ColumnName == PakFileViewColumns::CompressionBlockCountColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetCompressionBlockCount)
				];
		}
		else if (ColumnName == PakFileViewColumns::CompressionBlockSizeColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetCompressionBlockSize).ToolTipText(this, &SPakFileRow::GetCompressionBlockSizeToolTip)
				];
		}
		else if (ColumnName == PakFileViewColumns::SHA1ColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetSHA1)
				];
		}
		else if (ColumnName == PakFileViewColumns::IsEncryptedColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetIsEncrypted)
				];
		}
		else
		{
			return SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column"));
		}
	}

protected:

	FText GetName() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::FromString(FPaths::GetCleanFilename(PakFileItemPin->Filename));
		}
		else
		{
			return FText();
		}
	}

	FText GetPath() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::FromString(PakFileItemPin->Filename);
		}
		else
		{
			return FText();
		}
	}

	FText GetOffset() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->PakEntry)
		{
			return FText::AsNumber(PakFileItemPin->PakEntry->Offset);
		}
		else
		{
			return FText();
		}
	}

	FText GetSize() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->PakEntry)
		{
			return FText::AsMemory(PakFileItemPin->PakEntry->UncompressedSize, EMemoryUnitStandard::IEC);
		}
		else
		{
			return FText();
		}
	}

	FText GetSizeToolTip() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->PakEntry)
		{
			return FText::AsNumber(PakFileItemPin->PakEntry->UncompressedSize);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressedSize() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->PakEntry)
		{
			return FText::AsMemory(PakFileItemPin->PakEntry->Size, EMemoryUnitStandard::IEC);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressedSizeToolTip() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->PakEntry)
		{
			return FText::AsNumber(PakFileItemPin->PakEntry->Size);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressionBlockCount() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->PakEntry)
		{
			return FText::AsNumber(PakFileItemPin->PakEntry->CompressionBlocks.Num());
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressionBlockSize() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->PakEntry)
		{
			return FText::AsMemory(PakFileItemPin->PakEntry->CompressionBlockSize, EMemoryUnitStandard::IEC);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressionBlockSizeToolTip() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->PakEntry)
		{
			return FText::AsNumber(PakFileItemPin->PakEntry->CompressionBlockSize);
		}
		else
		{
			return FText();
		}
	}

	FText GetSHA1() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->PakEntry)
		{
			return FText::FromString(BytesToHex(PakFileItemPin->PakEntry->Hash, sizeof(PakFileItemPin->PakEntry->Hash)));
		}
		else
		{
			return FText();
		}
	}

	FText GetIsEncrypted() const
	{
		SPakFileView::FPakFileItem PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->PakEntry)
		{
			return FText::FromString(PakFileItemPin->PakEntry->IsEncrypted() ? TEXT("True") : TEXT("False"));
		}
		else
		{
			return FText();
		}
	}

protected:
	TWeakPtr<FPakFileEntry> WeakPakFileItem;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// SPakFileView
////////////////////////////////////////////////////////////////////////////////////////////////////

SPakFileView::SPakFileView()
{

}

SPakFileView::~SPakFileView()
{

}

void SPakFileView::Construct(const FArguments& InArgs)
{
	SAssignNew(ExternalScrollbar, SScrollBar).AlwaysShowScrollbar(true);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().VAlign(VAlign_Center).AutoHeight()
		[
			SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(2.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().VAlign(VAlign_Center).Padding(2.0f).AutoHeight()
				[
					SAssignNew(SearchBox, SSearchBox)
					.HintText(LOCTEXT("SearchBoxHint", "Search files"))
					.OnTextChanged(this, &SPakFileView::OnSearchBoxTextChanged)
					.IsEnabled(this, &SPakFileView::SearchBoxIsEnabled)
					.ToolTipText(LOCTEXT("FilterSearchHint", "Type here to search files"))
				]
			]

		]

		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SBox).VAlign(VAlign_Fill).HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f).VAlign(VAlign_Fill)
				[
					SNew(SScrollBox).Orientation(Orient_Horizontal)

					+ SScrollBox::Slot().VAlign(VAlign_Fill)
					[
						SNew(SBorder).BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder")).Padding(0.f)
						[
							SAssignNew(FileListView, SListView<FPakFileItem>)
							.ExternalScrollbar(ExternalScrollbar)
							.ItemHeight(20.f)
							.SelectionMode(ESelectionMode::Single)
							//.OnMouseButtonClick()
							//.OnSelectiongChanged()
							.ListItemsSource(&FileCache)
							.OnGenerateRow(this, &SPakFileView::OnGenerateFileRow)
							.ConsumeMouseWheel(EConsumeMouseWheel::Always)
							.OnContextMenuOpening(this, &SPakFileView::OnGenerateContextMenu)
							.HeaderRow
							(
								SAssignNew(FileListHeaderRow, SHeaderRow).Visibility(EVisibility::Visible)
							)
						]
					]
				]

				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f)
				[
					SNew(SBox).WidthOverride(FOptionalSize(13.f))
					[
						ExternalScrollbar.ToSharedRef()
					]
				]
			]
		]
	];

	InitializeAndShowHeaderColumns();
}

void SPakFileView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (FileCache.Num() <= 0)
	{
		const TArray<TSharedPtr<FPakFileEntry>>& AllFiles = IPakAnalyzerModule::Get().GetPakAnalyzer()->GetFiles();
		for (int32 Index = 0; Index < AllFiles.Num(); ++Index)
		{
			FileCache.Add(AllFiles[Index]);
			FileListView->RebuildList();
		}
	}

	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

bool SPakFileView::SearchBoxIsEnabled() const
{
	return true;
}

void SPakFileView::OnSearchBoxTextChanged(const FText& inFilterText)
{
	
}

TSharedRef<ITableRow> SPakFileView::OnGenerateFileRow(FPakFileItem InPakFileItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPakFileRow, InPakFileItem, OwnerTable);
}

TSharedPtr<SWidget> SPakFileView::OnGenerateContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	// Selection menu
	MenuBuilder.BeginSection("Operation", LOCTEXT("ContextMenu_Header_Operation", "Operation"));
	{
		FUIAction Action_JumpToTreeView
		(
			FExecuteAction::CreateSP(this, &SPakFileView::OnJumpToTreeViewExecute),
			FCanExecuteAction::CreateSP(this, &SPakFileView::HasFileSelected)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Columns_JumpToTreeView", "Jump To Tree View"),
			LOCTEXT("ContextMenu_Columns_JumpToTreeView_Desc", "Show Current Selected File In Tree View"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ResetColumn"), Action_JumpToTreeView, NAME_None, EUserInterfaceActionType::Button
		);

		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_Header_Columns_Copy", "Copy Column(s)"),
			LOCTEXT("ContextMenu_Header_Columns_Copy_Desc", "Copy value of column(s)"),
			FNewMenuDelegate::CreateSP(this, &SPakFileView::OnBuildCopyColumnMenu),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.CopyColumn")
		);
	}
	MenuBuilder.EndSection();

	// Columns menu
	MenuBuilder.BeginSection("Columns", LOCTEXT("ContextMenu_Header_Columns", "Columns"));
	{
		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_Header_Columns_View", "View Column"),
			LOCTEXT("ContextMenu_Header_Columns_View_Desc", "Hides or shows columns"),
			FNewMenuDelegate::CreateSP(this, &SPakFileView::OnBuildViewColumnMenu),
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ViewColumn")
		);

		FUIAction Action_ShowAllColumns
		(
			FExecuteAction::CreateSP(this, &SPakFileView::OnShowAllColumnsExecute),
			FCanExecuteAction::CreateSP(this, &SPakFileView::OnShowAllColumnsCanExecute)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Header_Columns_ShowAllColumns", "Show All Columns"),
			LOCTEXT("ContextMenu_Header_Columns_ShowAllColumns_Desc", "Resets tree view to show all columns"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ResetColumn"), Action_ShowAllColumns, NAME_None, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SPakFileView::OnBuildSortByMenu(FMenuBuilder& MenuBuilder)
{

}

void SPakFileView::OnBuildCopyColumnMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("CopyAllColumns", LOCTEXT("ContextMenu_Header_Columns_CopyAllColumns", "Copy All Columns"));
	{
		FUIAction Action_CopyAllColumns
		(
			FExecuteAction::CreateSP(this, &SPakFileView::OnCopyAllColumnsExecute),
			FCanExecuteAction::CreateSP(this, &SPakFileView::HasFileSelected)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Columns_CopyAllColumns", "Copy All Columns"),
			LOCTEXT("ContextMenu_Columns_CopyAllColumns_Desc", "Copy All Columns Of Current Selected File To Clipboard"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ResetColumn"), Action_CopyAllColumns, NAME_None, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("CopyColumn", LOCTEXT("ContextMenu_Header_Columns_CopySingleColumn", "Copy Column"));
	{
		for (const auto& ColumnPair : FileColumns)
		{
			const FFileColumn& Column = ColumnPair.Value;

			if (Column.IsVisible())
			{
				FUIAction Action_CopyColumn
				(
					FExecuteAction::CreateSP(this, &SPakFileView::OnCopyColumnExecute, Column.GetId()),
					FCanExecuteAction::CreateSP(this, &SPakFileView::HasFileSelected)
				);

				MenuBuilder.AddMenuEntry
				(
					FText::Format(LOCTEXT("ContextMenu_Columns_CopySingleColumn", "Copy {0}"), Column.GetTitleName()),
					FText::Format(LOCTEXT("ContextMenu_Columns_CopySingleColumn_Desc", "Copy {0} Of Current Selected File To Clipboard"), Column.GetTitleName()),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ResetColumn"), Action_CopyColumn, NAME_None, EUserInterfaceActionType::Button
				);
			}
		}
	}
	MenuBuilder.EndSection();
}

void SPakFileView::OnBuildViewColumnMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("ViewColumn", LOCTEXT("ContextMenu_Header_Columns_View", "View Column"));

	for (const auto& ColumnPair : FileColumns)
	{
		const FFileColumn& Column = ColumnPair.Value;

		FUIAction Action_ToggleColumn
		(
			FExecuteAction::CreateSP(this, &SPakFileView::ToggleColumnVisibility, Column.GetId()),
			FCanExecuteAction::CreateSP(this, &SPakFileView::CanToggleColumnVisibility, Column.GetId()),
			FIsActionChecked::CreateSP(this, &SPakFileView::IsColumnVisible, Column.GetId())
		);
		MenuBuilder.AddMenuEntry
		(
			Column.GetTitleName(),
			Column.GetDescription(),
			FSlateIcon(), Action_ToggleColumn, NAME_None, EUserInterfaceActionType::ToggleButton
		);
	}

	MenuBuilder.EndSection();
}

void SPakFileView::InitializeAndShowHeaderColumns()
{
	FileColumns.Empty();
	FileColumns.Emplace(PakFileViewColumns::NameColumnName, FFileColumn(0, PakFileViewColumns::NameColumnName, LOCTEXT("NameColumn", "Name"), LOCTEXT("NameColumnTip", "File name"), 270.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered));
	FileColumns.Emplace(PakFileViewColumns::PathColumnName, FFileColumn(1, PakFileViewColumns::PathColumnName, LOCTEXT("PathColumn", "Path"), LOCTEXT("PathColumnTip", "File path in pak"), 600.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	FileColumns.Emplace(PakFileViewColumns::OffsetColumnName, FFileColumn(2, PakFileViewColumns::OffsetColumnName, LOCTEXT("OffsetColumn", "Offset"), LOCTEXT("OffsetColumnTip", "File offset in pak"), 110.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	FileColumns.Emplace(PakFileViewColumns::SizeColumnName, FFileColumn(3, PakFileViewColumns::SizeColumnName, LOCTEXT("SizeColumn", "Size"), LOCTEXT("SizeColumnTip", "File original size"), 110.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	FileColumns.Emplace(PakFileViewColumns::CompressedSizeColumnName, FFileColumn(4, PakFileViewColumns::CompressedSizeColumnName, LOCTEXT("CompressedSizeColumn", "Compressed Size"), LOCTEXT("CompressedSizeColumnTip", "File compressed size"), 110.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	FileColumns.Emplace(PakFileViewColumns::CompressionBlockCountColumnName, FFileColumn(5, PakFileViewColumns::CompressionBlockCountColumnName, LOCTEXT("CompressionBlockCountColumn", "Compression Block Count"), LOCTEXT("CompressionBlockCountColumnTip", "File compression block count"), 155.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	FileColumns.Emplace(PakFileViewColumns::CompressionBlockSizeColumnName, FFileColumn(6, PakFileViewColumns::CompressionBlockSizeColumnName, LOCTEXT("CompressionBlockSizeColumn", "Compression Block Size"), LOCTEXT("CompressionBlockSizeColumnTip", "File compression block size"), 155.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeHidden));
	FileColumns.Emplace(PakFileViewColumns::SHA1ColumnName, FFileColumn(7, PakFileViewColumns::SHA1ColumnName, LOCTEXT("SHA1Column", "SHA1"), LOCTEXT("SHA1ColumnTip", "File sha1"), 315.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeHidden));
	FileColumns.Emplace(PakFileViewColumns::IsEncryptedColumnName, FFileColumn(8, PakFileViewColumns::IsEncryptedColumnName, LOCTEXT("IsEncryptedColumn", "IsEncrypted"), LOCTEXT("IsEncryptedColumnTip", "Is file encrypted in pak?"), 70.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeHidden));

	// Show columns.
	for (const auto& ColumnPair : FileColumns)
	{
		if (ColumnPair.Value.ShouldBeVisible())
		{
			ShowColumn(ColumnPair.Key);
		}
	}
}

SPakFileView::FFileColumn* SPakFileView::FindCoulum(const FName ColumnId)
{
	return FileColumns.Find(ColumnId);
}

EColumnSortMode::Type SPakFileView::GetSortModeForColumn(const FName ColumnId) const
{
	return EColumnSortMode::None;
}

void SPakFileView::OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode)
{

}

bool SPakFileView::CanShowColumn(const FName ColumnId) const
{
	return true;
}

void SPakFileView::ShowColumn(const FName ColumnId)
{
	FFileColumn* Column = FindCoulum(ColumnId);
	if (!Column)
	{
		return;
	}

	Column->Show();

	SHeaderRow::FColumn::FArguments ColumnArgs;
	ColumnArgs
		.ColumnId(Column->GetId())
		.DefaultLabel(Column->GetTitleName())
		.HAlignHeader(HAlign_Fill)
		.VAlignHeader(VAlign_Fill)
		.HeaderContentPadding(FMargin(2.f))
		.HAlignCell(HAlign_Fill)
		.VAlignCell(VAlign_Fill)
		.SortMode(this, &SPakFileView::GetSortModeForColumn, Column->GetId())
		.OnSort(this, &SPakFileView::OnSortModeChanged)
		.ManualWidth(Column->GetInitialWidth())
		.HeaderContent()
		[
			SNew(SBox)
			.ToolTipText(Column->GetDescription())
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(Column->GetTitleName())
			]
		];

	int32 ColumnIndex = 0;
	const int32 TargetColumnIndex = Column->GetIndex();
	const TIndirectArray<SHeaderRow::FColumn>& Columns = FileListHeaderRow->GetColumns();
	const int32 CurrentColumnCount = Columns.Num();
	for (; ColumnIndex < CurrentColumnCount; ++ColumnIndex)
	{
		const SHeaderRow::FColumn& CurrentColumn = Columns[ColumnIndex];
		FFileColumn* FileColumn = FindCoulum(CurrentColumn.ColumnId);
		if (TargetColumnIndex < FileColumn->GetIndex())
		{
			break;
		}
	}

	FileListHeaderRow->InsertColumn(ColumnArgs, ColumnIndex);
}

bool SPakFileView::CanHideColumn(const FName ColumnId)
{
	FFileColumn* Column = FindCoulum(ColumnId);

	return Column ? Column->CanBeHidden() : false;
}

void SPakFileView::HideColumn(const FName ColumnId)
{
	FFileColumn* Column = FindCoulum(ColumnId);
	if (Column)
	{
		Column->Hide();
		FileListHeaderRow->RemoveColumn(ColumnId);
	}
}

bool SPakFileView::IsColumnVisible(const FName ColumnId)
{
	const FFileColumn* Column = FindCoulum(ColumnId);

	return Column ? Column->IsVisible() : false;
}

bool SPakFileView::CanToggleColumnVisibility(const FName ColumnId)
{
	FFileColumn* Column = FindCoulum(ColumnId);

	return Column ? !Column->IsVisible() || Column->CanBeHidden() : false;
}

void SPakFileView::ToggleColumnVisibility(const FName ColumnId)
{
	FFileColumn* Column = FindCoulum(ColumnId);
	if (!Column)
	{
		return;
	}

	if (Column->IsVisible())
	{
		HideColumn(ColumnId);
	}
	else
	{
		ShowColumn(ColumnId);
	}
}

bool SPakFileView::OnShowAllColumnsCanExecute() const
{
	return true;
}

void SPakFileView::OnShowAllColumnsExecute()
{
	// Show columns.
	for (const auto& ColumnPair : FileColumns)
	{
		if (!ColumnPair.Value.IsVisible())
		{
			ShowColumn(ColumnPair.Key);
		}
	}
}

bool SPakFileView::HasFileSelected() const
{
	TArray<FPakFileItem> SelectedItems = FileListView->GetSelectedItems();

	return SelectedItems.Num() > 0;
}

void SPakFileView::OnCopyAllColumnsExecute()
{
	TArray<FPakFileItem> SelectedItems = FileListView->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		FPakFileItem PakFileItem = SelectedItems[0];
		if (PakFileItem.IsValid() && PakFileItem->PakEntry)
		{
			FString Value;
			Value = FString::Printf(TEXT("Filename: %s\n"), *FPaths::GetCleanFilename(PakFileItem->Filename));
			Value += FString::Printf(TEXT("Path: %s\n"), *PakFileItem->Filename);
			Value += FString::Printf(TEXT("Offset: %lld\n"), PakFileItem->PakEntry->Offset);
			Value += FString::Printf(TEXT("Size: %lld\n"), PakFileItem->PakEntry->UncompressedSize);
			Value += FString::Printf(TEXT("Compressed Size: %lld\n"), PakFileItem->PakEntry->Size);
			Value += FString::Printf(TEXT("Compression Block Count: %d\n"), PakFileItem->PakEntry->CompressionBlocks.Num());
			Value += FString::Printf(TEXT("Compression Block Size: %u\n"), PakFileItem->PakEntry->CompressionBlockSize);
			Value += FString::Printf(TEXT("SHA1: %s\n"), *BytesToHex(PakFileItem->PakEntry->Hash, sizeof(PakFileItem->PakEntry->Hash)));
			Value += FString::Printf(TEXT("Compressed Size: %s\n"), PakFileItem->PakEntry->IsEncrypted() ? TEXT("True") : TEXT("False"));

			FPlatformApplicationMisc::ClipboardCopy(*Value);
		}
	}
}

void SPakFileView::OnCopyColumnExecute(const FName ColumnId)
{
	TArray<FPakFileItem> SelectedItems = FileListView->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		FPakFileItem PakFileItem = SelectedItems[0];
		if (PakFileItem.IsValid() && PakFileItem->PakEntry)
		{
			FString Value;
			if (ColumnId == PakFileViewColumns::NameColumnName)
			{
				Value = FString::Printf(TEXT("Filename: %s"), *FPaths::GetCleanFilename(PakFileItem->Filename));
			}
			else if (ColumnId == PakFileViewColumns::PathColumnName)
			{
				Value = FString::Printf(TEXT("Path: %s"), *PakFileItem->Filename);
			}
			else if (ColumnId == PakFileViewColumns::OffsetColumnName)
			{
				Value = FString::Printf(TEXT("Offset: %lld"), PakFileItem->PakEntry->Offset);
			}
			else if (ColumnId == PakFileViewColumns::SizeColumnName)
			{
				Value = FString::Printf(TEXT("Size: %lld"), PakFileItem->PakEntry->UncompressedSize);
			}
			else if (ColumnId == PakFileViewColumns::CompressedSizeColumnName)
			{
				Value = FString::Printf(TEXT("Compressed Size: %lld"), PakFileItem->PakEntry->Size);
			}
			else if (ColumnId == PakFileViewColumns::CompressionBlockCountColumnName)
			{
				Value = FString::Printf(TEXT("Compression Block Count: %d"), PakFileItem->PakEntry->CompressionBlocks.Num());
			}
			else if (ColumnId == PakFileViewColumns::CompressionBlockSizeColumnName)
			{
				Value = FString::Printf(TEXT("Compression Block Size: %u"), PakFileItem->PakEntry->CompressionBlockSize);
			}
			else if (ColumnId == PakFileViewColumns::SHA1ColumnName)
			{
				Value = FString::Printf(TEXT("SHA1: %s"), *BytesToHex(PakFileItem->PakEntry->Hash, sizeof(PakFileItem->PakEntry->Hash)));
			}
			else if (ColumnId == PakFileViewColumns::IsEncryptedColumnName)
			{
				Value = FString::Printf(TEXT("Compressed Size: %s"), PakFileItem->PakEntry->IsEncrypted() ? TEXT("True") : TEXT("False"));
			}

			FPlatformApplicationMisc::ClipboardCopy(*Value);
		}
	}
}

void SPakFileView::OnJumpToTreeViewExecute()
{

}

#undef LOCTEXT_NAMESPACE
