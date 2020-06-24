#include "SPakFileView.h"

#include "EditorStyle.h"
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
							//.OnContextMenuOpening()
							.HeaderRow
							(
								SNew(SHeaderRow)

								+ SHeaderRow::Column(PakFileViewColumns::NameColumnName).ManualWidth(270.f).DefaultLabel(LOCTEXT("NameColumn", "Name"))
								+ SHeaderRow::Column(PakFileViewColumns::PathColumnName).ManualWidth(600.f).DefaultLabel(LOCTEXT("PathColumn", "Path"))
								+ SHeaderRow::Column(PakFileViewColumns::OffsetColumnName).ManualWidth(110.f).DefaultLabel(LOCTEXT("OffsetColumn", "Offset"))
								+ SHeaderRow::Column(PakFileViewColumns::SizeColumnName).ManualWidth(110.f).DefaultLabel(LOCTEXT("SizeColumn", "Size"))
								+ SHeaderRow::Column(PakFileViewColumns::CompressedSizeColumnName).ManualWidth(110.f).DefaultLabel(LOCTEXT("CompressedSizeColumn", "Compressed Size"))
								+ SHeaderRow::Column(PakFileViewColumns::CompressionBlockCountColumnName).ManualWidth(155.f).DefaultLabel(LOCTEXT("CompressionBlockCountColumn", "Compression Block Count"))
								+ SHeaderRow::Column(PakFileViewColumns::CompressionBlockSizeColumnName).ManualWidth(155.f).DefaultLabel(LOCTEXT("CompressionBlockSizeColumn", "Compression Block Size"))
								+ SHeaderRow::Column(PakFileViewColumns::SHA1ColumnName).ManualWidth(315.f).DefaultLabel(LOCTEXT("SHA1Column", "SHA1"))
								+ SHeaderRow::Column(PakFileViewColumns::IsEncryptedColumnName).ManualWidth(70.f).DefaultLabel(LOCTEXT("IsEncryptedColumn", "IsEncrypted"))
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

#undef LOCTEXT_NAMESPACE
