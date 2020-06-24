#include "SPakFileView.h"

#include "EditorStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

#define LOCTEXT_NAMESPACE "SPakFileView"

////////////////////////////////////////////////////////////////////////////////////////////////////
// SPakFileRow
////////////////////////////////////////////////////////////////////////////////////////////////////

class SPakFileRow : public SMultiColumnTableRow<TSharedPtr<FPakFileRef>>
{
	SLATE_BEGIN_ARGS(SPakFileRow) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<FPakFileRef> InPakFileRef, TSharedPtr<SPakFileView> InParentWidget, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		WeakPakFileRef = MoveTemp(InPakFileRef);
		WeakParentWidget = InParentWidget;

		SMultiColumnTableRow<TSharedPtr<FPakFileRef>>::Construct(FSuperRowType::FArguments(), InOwnerTableView);

		TSharedRef<SWidget> Row = ChildSlot.GetChildAt(0);

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(new FSlateColorBrush(FLinearColor::White))
			.BorderBackgroundColor(this, &SPakFileRow::GetBackgroundColor)
			[
				Row
			]
		];
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		return SNullWidget::NullWidget;
	}

protected:
	FSlateColor GetBackgroundColor() const
	{
		TSharedPtr<SPakFileView> ParentWidgetPin = WeakParentWidget.Pin();
		TSharedPtr<FPakFileRef> PakFileRefPin = WeakPakFileRef.Pin();
		//if (ParentWidgetPin.IsValid() && PakFileRefPin.IsValid())
		//{
		//	TSharedPtr<FLogMessage> SelectedLogMessage = ParentWidgetPin->GetSelectedLogMessage();
		//	if (!SelectedLogMessage || SelectedLogMessage->GetIndex() != LogMessagePin->GetIndex()) // if row is not selected
		//	{
		//		FLogMessageRecord& CacheEntry = ParentWidgetPin->GetCache().Get(LogMessagePin->GetIndex());
		//		const double Time = CacheEntry.Time;

		//		TSharedPtr<STimingProfilerWindow> Window = FTimingProfilerManager::Get()->GetProfilerWindow();
		//		if (Window)
		//		{
		//			TSharedPtr<STimingView> TimingView = Window->GetTimingView();
		//			if (TimingView)
		//			{
		//				if (TimingView->IsTimeSelectedInclusive(Time))
		//				{
		//					return FSlateColor(FLinearColor(0.25f, 0.5f, 1.0f, 0.25f));
		//				}
		//			}
		//		}
		//	}
		//}

		return FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	}

protected:
	TWeakPtr<FPakFileRef> WeakPakFileRef;
	TWeakPtr<SPakFileView> WeakParentWidget;
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
	SAssignNew(ExternalScrollbar, SScrollBar);

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
							SAssignNew(FileListView, SListView<TSharedPtr<FPakFileRef>>)
							.ExternalScrollbar(ExternalScrollbar)
							.ItemHeight(20.f)
							.SelectionMode(ESelectionMode::Single)
							//.OnMouseButtonClick()
							//.OnSelectiongChanged()
							.ListItemsSource(&PakFiles)
							.OnGenerateRow(this, &SPakFileView::OnGenerateFileRow)
							.ConsumeMouseWheel(EConsumeMouseWheel::Always)
							//.OnContextMenuOpening()
							.HeaderRow
							(
								SNew(SHeaderRow)

								+ SHeaderRow::Column(PakFileViewColumns::IndexColumnName).ManualWidth(30.f).DefaultLabel(LOCTEXT("IndexColumn", "Index"))
								+ SHeaderRow::Column(PakFileViewColumns::NameColumnName).ManualWidth(90.f).DefaultLabel(LOCTEXT("NameColumn", "Name"))
								+ SHeaderRow::Column(PakFileViewColumns::PathColumnName).ManualWidth(270.f).DefaultLabel(LOCTEXT("PathColumn", "Path"))
								+ SHeaderRow::Column(PakFileViewColumns::OffsetColumnName).ManualWidth(60.f).DefaultLabel(LOCTEXT("OffsetColumn", "Offset"))
								+ SHeaderRow::Column(PakFileViewColumns::SizeColumnName).ManualWidth(60.f).DefaultLabel(LOCTEXT("SizeColumn", "Size"))
								+ SHeaderRow::Column(PakFileViewColumns::CompressedSizeColumnName).ManualWidth(60.f).DefaultLabel(LOCTEXT("CompressedSizeColumn", "CompressedSize"))
								+ SHeaderRow::Column(PakFileViewColumns::CompressionMethodColumnName).ManualWidth(60.f).DefaultLabel(LOCTEXT("CompressionMethodColumn", "CompressionMethod"))
								+ SHeaderRow::Column(PakFileViewColumns::CompressionBlockCountColumnName).ManualWidth(30.f).DefaultLabel(LOCTEXT("CompressionBlockCountColumn", "CompressionBlockCount"))
								+ SHeaderRow::Column(PakFileViewColumns::CompressionBlockSizeColumnName).ManualWidth(60.f).DefaultLabel(LOCTEXT("CompressionBlockSizeColumn", "CompressionBlockSize"))
								+ SHeaderRow::Column(PakFileViewColumns::SHA1ColumnName).ManualWidth(120.f).DefaultLabel(LOCTEXT("SHA1Column", "SHA1"))
								+ SHeaderRow::Column(PakFileViewColumns::IsEncryptedColumnName).ManualWidth(30.f).DefaultLabel(LOCTEXT("IsEncryptedColumn", "IsEncrypted"))
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

bool SPakFileView::SearchBoxIsEnabled() const
{
	return true;
}

void SPakFileView::OnSearchBoxTextChanged(const FText& inFilterText)
{
	
}

TSharedRef<ITableRow> SPakFileView::OnGenerateFileRow(TSharedPtr<FPakFileRef> InPakFileRef, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPakFileRow, InPakFileRef, SharedThis(this), OwnerTable);
}

#undef LOCTEXT_NAMESPACE
