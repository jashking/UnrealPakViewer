#include "SPakClassView.h"

#include "EditorStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

#include "UnrealPakViewerStyle.h"

#define LOCTEXT_NAMESPACE "SPakClassView"

////////////////////////////////////////////////////////////////////////////////////////////////////
// SPakClassRow
////////////////////////////////////////////////////////////////////////////////////////////////////

class SPakClassRow : public SMultiColumnTableRow<FPakClassEntryPtr>
{
	SLATE_BEGIN_ARGS(SPakClassRow) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, FPakClassEntryPtr InPakClassItem, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		if (!InPakClassItem.IsValid())
		{
			return;
		}

		WeakPakClassItem = MoveTemp(InPakClassItem);

		SMultiColumnTableRow<FPakClassEntryPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);

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
		if (ColumnName == FClassColumn::ClassColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakClassRow::GetName).ToolTipText(this, &SPakClassRow::GetName).ColorAndOpacity(this, &SPakClassRow::GetNameColor)
				];
		}
		else if (ColumnName == FClassColumn::SizeColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakClassRow::GetSize).ToolTipText(this, &SPakClassRow::GetSizeToolTip)
				];
		}
		else if (ColumnName == FClassColumn::CompressedSizeColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakClassRow::GetCompressedSize).ToolTipText(this, &SPakClassRow::GetCompressedSizeToolTip)
				];
		}
		else if (ColumnName == FClassColumn::FileCountColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakClassRow::GetFileCount)
				];
		}
		else if (ColumnName == FClassColumn::PercentOfTotalColumnName)
		{
			return
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SNew(SProgressBar).Percent(this, &SPakClassRow::GetPercentOfTotal)
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SPakClassRow::GetPercentOfTotalText)
					.ColorAndOpacity(FLinearColor::Black)
				];
		}
		else if (ColumnName == FClassColumn::PercentOfParentColumnName)
		{
			return
				SNew(SOverlay)

				+ SOverlay::Slot()
				[
					SNew(SProgressBar).Percent(this, &SPakClassRow::GetPercentOfParent)
				]

				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SPakClassRow::GetPercentOfParentText)
					.ColorAndOpacity(FLinearColor::Black)
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
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return FText::FromName(PakClassItemPin->Class);
		}
		else
		{
			return FText();
		}
	}

	FSlateColor GetNameColor() const
	{
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return FSlateColor(FClassColumn::GetColorByClass(*PakClassItemPin->Class.ToString()));
		}
		else
		{
			return FLinearColor::White;
		}
	}

	FText GetSize() const
	{
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return FText::AsMemory(PakClassItemPin->Size, EMemoryUnitStandard::IEC);
		}
		else
		{
			return FText();
		}
	}

	FText GetSizeToolTip() const
	{
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return FText::AsNumber(PakClassItemPin->Size);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressedSize() const
	{
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return FText::AsMemory(PakClassItemPin->CompressedSize, EMemoryUnitStandard::IEC);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressedSizeToolTip() const
	{
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return FText::AsNumber(PakClassItemPin->CompressedSize);
		}
		else
		{
			return FText();
		}
	}
	
	FText GetFileCount() const
	{
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return FText::AsNumber(PakClassItemPin->FileCount);
		}
		else
		{
			return FText();
		}
	}

	TOptional<float> GetPercentOfTotal() const
	{
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return PakClassItemPin->PercentOfTotal;
		}
		else
		{
			return TOptional<float>();
		}
	}

	FText GetPercentOfTotalText() const
	{
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return FText::FromString(FString::Printf(TEXT("%.2f%%"), PakClassItemPin->PercentOfTotal * 100));
		}
		else
		{
			return FText();
		}
	}

	TOptional<float> GetPercentOfParent() const
	{
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return PakClassItemPin->PercentOfParent;
		}
		else
		{
			return TOptional<float>();
		}
	}

	FText GetPercentOfParentText() const
	{
		FPakClassEntryPtr PakClassItemPin = WeakPakClassItem.Pin();
		if (PakClassItemPin.IsValid())
		{
			return FText::FromString(FString::Printf(TEXT("%.2f%%"), PakClassItemPin->PercentOfParent * 100));
		}
		else
		{
			return FText();
		}
	}
protected:
	TWeakPtr<FPakClassEntry> WeakPakClassItem;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// SPakClassView
////////////////////////////////////////////////////////////////////////////////////////////////////

SPakClassView::SPakClassView()
{

}

SPakClassView::~SPakClassView()
{
}

void SPakClassView::Construct(const FArguments& InArgs)
{
	SAssignNew(ExternalScrollbar, SScrollBar).AlwaysShowScrollbar(false);

	ChildSlot
	[
		SNew(SVerticalBox)

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
							SAssignNew(ClassListView, SListView<FPakClassEntryPtr>)
							.ExternalScrollbar(ExternalScrollbar)
							.ItemHeight(20.f)
							.SelectionMode(ESelectionMode::Multi)
							.ListItemsSource(&ClassCache)
							.OnGenerateRow(this, &SPakClassView::OnGenerateClassRow)
							.ConsumeMouseWheel(EConsumeMouseWheel::Always)
							.HeaderRow
							(
								SAssignNew(ClassListHeaderRow, SHeaderRow).Visibility(EVisibility::Visible)
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

	LastLoadGuid = LexToString(FGuid());
}

void SPakClassView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SPakClassView::Reload(FPakTreeEntryPtr InFolder)
{
	ClassCache.Empty();

	if (InFolder.IsValid())
	{
		for (const auto& Pair : InFolder->FileClassMap)
		{
			ClassCache.Add(Pair.Value);
		}
	}

	Sort();
	ClassListView->RebuildList();
}

TSharedRef<ITableRow> SPakClassView::OnGenerateClassRow(FPakClassEntryPtr InPakClassItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPakClassRow, InPakClassItem, OwnerTable);
}

void SPakClassView::InitializeAndShowHeaderColumns()
{
	ClassColumns.Empty();

	ClassColumns.Emplace(FClassColumn::ClassColumnName, FClassColumn(0, FClassColumn::ClassColumnName, LOCTEXT("ClassColumn", "Class"), LOCTEXT("ClassColumnTip", "Class name in asset registry or file extension if not found"), 200.f));
	ClassColumns.Emplace(FClassColumn::PercentOfTotalColumnName, FClassColumn(1, FClassColumn::PercentOfTotalColumnName, LOCTEXT("PercentOfTotalColumnName", "Percent Of Total"), LOCTEXT("PercentOfTotalColumnTip", "Percent of total compressed file size in pak"), 250.f));
	ClassColumns.Emplace(FClassColumn::PercentOfParentColumnName, FClassColumn(2, FClassColumn::PercentOfParentColumnName, LOCTEXT("PercentOfParentColumnName", "Percent Of Parent"), LOCTEXT("PercentOfParentColumnTip", "Percent of parent folder's compressed file size in pak"), 250.f));
	ClassColumns.Emplace(FClassColumn::CompressedSizeColumnName, FClassColumn(3, FClassColumn::CompressedSizeColumnName, LOCTEXT("CompressedSizeColumn", "Compressed Size"), LOCTEXT("CompressedSizeColumnTip", "Total compressed file size of this class in pak"), 150.f));
	ClassColumns.Emplace(FClassColumn::SizeColumnName, FClassColumn(4, FClassColumn::SizeColumnName, LOCTEXT("SizeColumn", "Size"), LOCTEXT("SizeColumnTip", "Total original file size of this class"), 150.f));
	ClassColumns.Emplace(FClassColumn::FileCountColumnName, FClassColumn(5, FClassColumn::FileCountColumnName, LOCTEXT("FileCountColumn", "File Count"), LOCTEXT("FileCountColumnTip", "Total file count of this class in pak"), 150.f));

	// Show columns.
	for (const auto& ColumnPair : ClassColumns)
	{
		ShowColumn(ColumnPair.Key);
	}
}

void SPakClassView::ShowColumn(const FName ColumnId)
{
	FClassColumn* Column = FindCoulum(ColumnId);
	if (!Column)
	{
		return;
	}

	SHeaderRow::FColumn::FArguments ColumnArgs;
	ColumnArgs
		.ColumnId(Column->GetId())
		.DefaultLabel(Column->GetTitleName())
		.HAlignHeader(HAlign_Fill)
		.VAlignHeader(VAlign_Fill)
		.HeaderContentPadding(FMargin(2.f))
		.HAlignCell(HAlign_Fill)
		.VAlignCell(VAlign_Fill)
		.SortMode(this, &SPakClassView::GetSortModeForColumn, Column->GetId())
		.OnSort(this, &SPakClassView::OnSortModeChanged)
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

	ClassListHeaderRow->InsertColumn(ColumnArgs, Column->GetIndex());
}

FClassColumn* SPakClassView::FindCoulum(const FName ColumnId)
{
	return ClassColumns.Find(ColumnId);
}

EColumnSortMode::Type SPakClassView::GetSortModeForColumn(const FName ColumnId) const
{
	if (CurrentSortedColumn != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return CurrentSortMode;
}

void SPakClassView::OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	FClassColumn* Column = FindCoulum(ColumnId);
	if (!Column)
	{
		return;
	}

	CurrentSortedColumn = ColumnId;
	CurrentSortMode = SortMode;
	Sort();
	ClassListView->RebuildList();
}

void SPakClassView::MarkDirty(bool bInIsDirty)
{
	bIsDirty = bInIsDirty;
}

void SPakClassView::Sort()
{
	ClassCache.Sort([this](const FPakClassEntryPtr& A, const FPakClassEntryPtr& B) -> bool
		{
			if (CurrentSortedColumn == FClassColumn::ClassColumnName)
			{
				if (CurrentSortMode == EColumnSortMode::Ascending)
				{
					return A->Class.LexicalLess(B->Class);
				}
				else
				{
					return !A->Class.LexicalLess(B->Class);
				}
			}
			else if (CurrentSortedColumn == FClassColumn::FileCountColumnName)
			{
				if (CurrentSortMode == EColumnSortMode::Ascending)
				{
					return A->FileCount < B->FileCount;
				}
				else
				{
					return A->FileCount > B->FileCount;
				}
			}
			else if (CurrentSortedColumn == FClassColumn::SizeColumnName)
			{
				if (CurrentSortMode == EColumnSortMode::Ascending)
				{
					return A->Size < B->Size;
				}
				else
				{
					return A->Size > B->Size;
				}
			}
			else if (CurrentSortedColumn == FClassColumn::CompressedSizeColumnName)
			{
				if (CurrentSortMode == EColumnSortMode::Ascending)
				{
					return A->CompressedSize < B->CompressedSize;
				}
				else
				{
					return A->CompressedSize > B->CompressedSize;
				}
			}
			else if (CurrentSortedColumn == FClassColumn::PercentOfTotalColumnName)
			{
				if (CurrentSortMode == EColumnSortMode::Ascending)
				{
					return A->PercentOfTotal < B->PercentOfTotal;
				}
				else
				{
					return A->PercentOfTotal > B->PercentOfTotal;
				}
			}
			else if (CurrentSortedColumn == FClassColumn::PercentOfParentColumnName)
			{
				if (CurrentSortMode == EColumnSortMode::Ascending)
				{
					return A->PercentOfParent < B->PercentOfParent;
				}
				else
				{
					return A->PercentOfParent > B->PercentOfParent;
				}
			}

			return false;
		});
}

#undef LOCTEXT_NAMESPACE
