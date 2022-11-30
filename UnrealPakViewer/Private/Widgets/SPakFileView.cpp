#include "SPakFileView.h"

#include "Async/AsyncWork.h"
#include "Async/TaskGraphInterfaces.h"
#include "DesktopPlatformModule.h"
//#include "EditorStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Json.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IPlatformFilePak.h"
#include "Misc/Guid.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

#include "CommonDefines.h"
#include "PakAnalyzerModule.h"
#include "UnrealPakViewerStyle.h"
#include "ViewModels/ClassColumn.h"
#include "ViewModels/FileSortAndFilter.h"
#include "ViewModels/WidgetDelegates.h"

#define LOCTEXT_NAMESPACE "SPakFileView"

////////////////////////////////////////////////////////////////////////////////////////////////////
// SPakFileRow
////////////////////////////////////////////////////////////////////////////////////////////////////

class SPakFileRow : public SMultiColumnTableRow<FPakFileEntryPtr>
{
	SLATE_BEGIN_ARGS(SPakFileRow) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, FPakFileEntryPtr InPakFileItem, TSharedRef<SPakFileView> InParentWidget, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		if (!InPakFileItem.IsValid())
		{
			return;
		}

		WeakPakFileItem = MoveTemp(InPakFileItem);
		WeakPakFileView = InParentWidget;

		SMultiColumnTableRow<FPakFileEntryPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);

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
		if (ColumnName == FFileColumn::NameColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetName).ToolTipText(this, &SPakFileRow::GetName).HighlightText(this, &SPakFileRow::GetSearchHighlightText)
				];
		}
		else if (ColumnName == FFileColumn::PathColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetPath).ToolTipText(this, &SPakFileRow::GetPath).HighlightText(this, &SPakFileRow::GetSearchHighlightText)
				];
		}
		else if (ColumnName == FFileColumn::ClassColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetClass).ColorAndOpacity(this, &SPakFileRow::GetClassColor)
				];
		}
		else if (ColumnName == FFileColumn::OffsetColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetOffset)
				];
		}
		else if (ColumnName == FFileColumn::SizeColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetSize).ToolTipText(this, &SPakFileRow::GetSizeToolTip)
				];
		}
		else if (ColumnName == FFileColumn::CompressedSizeColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetCompressedSize).ToolTipText(this, &SPakFileRow::GetCompressedSizeToolTip)
				];
		}
		else if (ColumnName == FFileColumn::CompressionBlockCountColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetCompressionBlockCount)
				];
		}
		else if (ColumnName == FFileColumn::CompressionBlockSizeColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetCompressionBlockSize).ToolTipText(this, &SPakFileRow::GetCompressionBlockSizeToolTip)
				];
		}
		else if (ColumnName == FFileColumn::CompressionMethodColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetCompressionMethod)
				];
		}
		else if (ColumnName == FFileColumn::SHA1ColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetSHA1).ToolTipText(this, &SPakFileRow::GetSHA1)
				];
		}
		else if (ColumnName == FFileColumn::IsEncryptedColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetIsEncrypted)
				];
		}
		else if (ColumnName == FFileColumn::OwnerPakColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetOwnerPakName).ToolTipText(this, &SPakFileRow::GetOwnerPakPath)
				];
		}
		else if (ColumnName == FFileColumn::DependencyCountColumnName)
		{
			return
				SNew(SBox).Padding(FMargin(4.0, 0.0))
				[
					SNew(STextBlock).Text(this, &SPakFileRow::GetDepenedencyCount)
				];
		}
		else if (ColumnName == FFileColumn::DependentCountColumnName)
		{
		return
			SNew(SBox).Padding(FMargin(4.0, 0.0))
			[
				SNew(STextBlock).Text(this, &SPakFileRow::GetDependentCount)
			];
		}
		else
		{
			return SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column"));
		}
	}

protected:

	FText GetSearchHighlightText() const
	{
		TSharedPtr<SPakFileView> ParentWidgetPin = WeakPakFileView.Pin();
		
		return ParentWidgetPin.IsValid() ? ParentWidgetPin->GetSearchText() : FText();
	}

	FText GetName() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::FromName(PakFileItemPin->Filename);
		}
		else
		{
			return FText();
		}
	}

	FText GetPath() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::FromString(PakFileItemPin->Path);
		}
		else
		{
			return FText();
		}
	}

	FText GetClass() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::FromName(PakFileItemPin->Class);
		}
		else
		{
			return FText();
		}
	}

	FSlateColor GetClassColor() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FSlateColor(FClassColumn::GetColorByClass(*PakFileItemPin->Class.ToString()));
		}
		else
		{
			return FLinearColor::White;
		}
	}

	FText GetOffset() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::AsNumber(PakFileItemPin->PakEntry.Offset);
		}
		else
		{
			return FText();
		}
	}

	FText GetSize() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::AsMemory(PakFileItemPin->PakEntry.UncompressedSize, EMemoryUnitStandard::IEC);
		}
		else
		{
			return FText();
		}
	}

	FText GetSizeToolTip() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::AsNumber(PakFileItemPin->PakEntry.UncompressedSize);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressedSize() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::AsMemory(PakFileItemPin->PakEntry.Size, EMemoryUnitStandard::IEC);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressedSizeToolTip() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::AsNumber(PakFileItemPin->PakEntry.Size);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressionBlockCount() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::AsNumber(PakFileItemPin->PakEntry.CompressionBlocks.Num());
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressionBlockSize() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::AsMemory(PakFileItemPin->PakEntry.CompressionBlockSize, EMemoryUnitStandard::IEC);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressionBlockSizeToolTip() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::AsNumber(PakFileItemPin->PakEntry.CompressionBlockSize);
		}
		else
		{
			return FText();
		}
	}

	FText GetCompressionMethod() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::FromName(PakFileItemPin->CompressionMethod);
		}
		else
		{
			return FText();
		}
	}

	FText GetSHA1() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::FromString(BytesToHex(PakFileItemPin->PakEntry.Hash, sizeof(PakFileItemPin->PakEntry.Hash)));
		}
		else
		{
			return FText();
		}
	}

	FText GetIsEncrypted() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			return FText::FromString(PakFileItemPin->PakEntry.IsEncrypted() ? TEXT("True") : TEXT("False"));
		}
		else
		{
			return FText();
		}
	}

	FText GetOwnerPakName() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			const TArray<FPakFileSumaryPtr>& Summaries = IPakAnalyzerModule::Get().GetPakAnalyzer()->GetPakFileSumary();
			if (Summaries.IsValidIndex(PakFileItemPin->OwnerPakIndex))
			{
				return FText::FromString(FPaths::GetCleanFilename(Summaries[PakFileItemPin->OwnerPakIndex]->PakFilePath));
			}
		}

		return FText();
	}

	FText GetOwnerPakPath() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid())
		{
			const TArray<FPakFileSumaryPtr>& Summaries = IPakAnalyzerModule::Get().GetPakAnalyzer()->GetPakFileSumary();
			if (Summaries.IsValidIndex(PakFileItemPin->OwnerPakIndex))
			{
				return FText::FromString(Summaries[PakFileItemPin->OwnerPakIndex]->PakFilePath);
			}
		}

		return FText();
	}

	FText GetDepenedencyCount() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->AssetSummary.IsValid())
		{
			return FText::AsNumber(PakFileItemPin->AssetSummary->DependencyList.Num());
		}

		return FText::AsNumber(0);
	}

	FText GetDependentCount() const
	{
		FPakFileEntryPtr PakFileItemPin = WeakPakFileItem.Pin();
		if (PakFileItemPin.IsValid() && PakFileItemPin->AssetSummary.IsValid())
		{
			return FText::AsNumber(PakFileItemPin->AssetSummary->DependentList.Num());
		}

		return FText::AsNumber(0);
	}
protected:
	TWeakPtr<FPakFileEntry> WeakPakFileItem;
	TWeakPtr<SPakFileView> WeakPakFileView;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// SPakFileView
////////////////////////////////////////////////////////////////////////////////////////////////////

SPakFileView::SPakFileView()
{
	FWidgetDelegates::GetOnLoadAssetRegistryFinishedDelegate().AddRaw(this, &SPakFileView::OnLoadAssetReigstryFinished);
	FPakAnalyzerDelegates::OnPakLoadFinish.AddRaw(this, &SPakFileView::OnLoadPakFinished);
	FPakAnalyzerDelegates::OnAssetParseFinish.AddRaw(this, &SPakFileView::OnParseAssetFinished);
}

SPakFileView::~SPakFileView()
{
	FWidgetDelegates::GetOnLoadAssetRegistryFinishedDelegate().RemoveAll(this);
	FPakAnalyzerDelegates::OnPakLoadFinish.RemoveAll(this);
	FPakAnalyzerDelegates::OnAssetParseFinish.RemoveAll(this);

	if (SortAndFilterTask.IsValid())
	{
		SortAndFilterTask->Cancel();
		SortAndFilterTask->EnsureCompletion();
	}
}

void SPakFileView::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().VAlign(VAlign_Center).AutoHeight()
		[
			SNew(SBorder)//.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(2.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().VAlign(VAlign_Center).Padding(2.0f).AutoHeight()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().Padding(0.f, 0.f, 5.f, 0.f).VAlign(VAlign_Center)
					[
						SNew(SComboButton)
						//.ComboButtonStyle(FEditorStyle::Get(), "GenericFilters.ComboButtonStyle")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(0)
						.ToolTipText(LOCTEXT("ClassFilterToolTip", "Filter files by class."))
						.OnGetMenuContent(this, &SPakFileView::OnBuildClassFilterMenu)
						.HasDownArrow(true)
						.ContentPadding(FMargin(1.0f, 1.0f, 1.0f, 0.0f))
						.ButtonContent()
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ClassFilter", "Class Filter")).ColorAndOpacity(FLinearColor::Green).ShadowOffset(FVector2D(1.f, 1.f))
							]
						]
					]

					+ SHorizontalBox::Slot().Padding(0.f, 0.f, 5.f, 0.f).VAlign(VAlign_Center)
					[
						SNew(SComboButton)
						//.ComboButtonStyle(FEditorStyle::Get(), "GenericFilters.ComboButtonStyle")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(0)
						.ToolTipText(LOCTEXT("PakFilterToolTip", "Filter files by pak."))
						.OnGetMenuContent(this, &SPakFileView::OnBuildPakFilterMenu)
						.HasDownArrow(true)
						.ContentPadding(FMargin(1.0f, 1.0f, 1.0f, 0.0f))
						.ButtonContent()
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(LOCTEXT("PakFilter", "Pak Filter")).ColorAndOpacity(FLinearColor::Green).ShadowOffset(FVector2D(1.f, 1.f))
							]
						]
					]

					+ SHorizontalBox::Slot()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f)
						[
							SAssignNew(SearchBox, SSearchBox)
							.HintText(LOCTEXT("SearchBoxHint", "Search files"))
							.OnTextChanged(this, &SPakFileView::OnSearchBoxTextChanged)
							.IsEnabled(this, &SPakFileView::SearchBoxIsEnabled)
							.ToolTipText(LOCTEXT("FilterSearchHint", "Type here to search files"))
						]

						+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(this, &SPakFileView::GetFileCount)
						]
					]
				]
			]

		]

		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SBorder)//.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(0.f)
			[
				SAssignNew(FileListView, SListView<FPakFileEntryPtr>)
				.ItemHeight(20.f)
				.SelectionMode(ESelectionMode::Multi)
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
	];

	InitializeAndShowHeaderColumns();

	SortAndFilterTask = MakeUnique<FAsyncTask<FFileSortAndFilterTask>>(CurrentSortedColumn, CurrentSortMode, SharedThis(this));
	InnderTask = SortAndFilterTask.IsValid() ? &SortAndFilterTask->GetTask() : nullptr;

	check(SortAndFilterTask.IsValid());
	check(InnderTask);

	InnderTask->GetOnSortAndFilterFinishedDelegate().BindRaw(this, &SPakFileView::OnSortAndFilterFinihed);

	FilesSummary = MakeShared<FPakFileEntry>(TEXT("Total"), TEXT("Total"));
}

void SPakFileView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	if (PakAnalyzer)
	{
		if (bIsDirty)
		{
			if (SortAndFilterTask->IsDone())
			{
				MarkDirty(false);

				TMap<int32, bool> IndexFilterMap;
				for (int32 i = 0; i < PakFilterMap.Num(); ++i)
				{
					IndexFilterMap.Add(i, PakFilterMap[i].bShow);
				}

				InnderTask->SetWorkInfo(CurrentSortedColumn, CurrentSortMode, CurrentSearchText, ClassFilterMap, IndexFilterMap);
				SortAndFilterTask->StartBackgroundTask();
			}
		}

		if (!DelayHighlightItem.IsEmpty() && !IsFileListEmpty())
		{
			ScrollToItem(DelayHighlightItem, DelayHighlightItemPakIndex);
			DelayHighlightItem = TEXT("");
			DelayHighlightItemPakIndex = -1;
		}
	}

	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

bool SPakFileView::SearchBoxIsEnabled() const
{
	return true;
}

void SPakFileView::OnSearchBoxTextChanged(const FText& InFilterText)
{
	if (CurrentSearchText.Equals(InFilterText.ToString(), ESearchCase::IgnoreCase))
	{
		return;
	}

	CurrentSearchText = InFilterText.ToString();
	MarkDirty(true);
}

TSharedRef<ITableRow> SPakFileView::OnGenerateFileRow(FPakFileEntryPtr InPakFileItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPakFileRow, InPakFileItem, SharedThis(this), OwnerTable);
}

TSharedPtr<SWidget> SPakFileView::OnGenerateContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	// Extract menu
	MenuBuilder.BeginSection("Extract", LOCTEXT("ContextMenu_Header_Extract", "Extract"));
	{
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Extract", "Extract..."),
			LOCTEXT("ContextMenu_Extract_Desc", "Extract selected files to disk"),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Extract"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SPakFileView::OnExtract),
				FCanExecuteAction::CreateSP(this, &SPakFileView::HasFileSelected)
			),
			NAME_None, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	// Export menu
	MenuBuilder.BeginSection("Export", LOCTEXT("ContextMenu_Header_Export", "Export File Info"));
	{
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Export_To_Json", "Export To Json..."),
			LOCTEXT("ContextMenu_Export_To_Json_Desc", "Export selected file(s) info to json"),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Export"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SPakFileView::OnExportToJson),
				FCanExecuteAction::CreateSP(this, &SPakFileView::HasFileSelected)
			),
			NAME_None, EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Export_To_Csv", "Export To Csv..."),
			LOCTEXT("ContextMenu_Export_To_Csv_Desc", "Export selected file(s) info to csv"),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Export"),
			FUIAction
			(
				FExecuteAction::CreateSP(this, &SPakFileView::OnExportToCsv),
				FCanExecuteAction::CreateSP(this, &SPakFileView::HasFileSelected)
			),
			NAME_None, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	// Operation menu
	MenuBuilder.BeginSection("Operation", LOCTEXT("ContextMenu_Header_Operation", "Operation"));
	{
		FUIAction Action_JumpToTreeView
		(
			FExecuteAction::CreateSP(this, &SPakFileView::OnJumpToTreeViewExecute),
			FCanExecuteAction::CreateSP(this, &SPakFileView::HasOneFileSelected)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Columns_JumpToTreeView", "Show In Tree View"),
			LOCTEXT("ContextMenu_Columns_JumpToTreeView_Desc", "Show current selected file in tree view"),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Find"), Action_JumpToTreeView, NAME_None, EUserInterfaceActionType::Button
		);

		MenuBuilder.AddSubMenu
		(
			LOCTEXT("ContextMenu_Header_Columns_Copy", "Copy Column(s)"),
			LOCTEXT("ContextMenu_Header_Columns_Copy_Desc", "Copy value of column(s)"),
			FNewMenuDelegate::CreateSP(this, &SPakFileView::OnBuildCopyColumnMenu),
			false,
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Copy")
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
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "View")
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
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "View"), Action_ShowAllColumns, NAME_None, EUserInterfaceActionType::Button
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
			LOCTEXT("ContextMenu_Columns_CopyAllColumns_Desc", "Copy all columns of selected file to clipboard"),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Copy"), Action_CopyAllColumns, NAME_None, EUserInterfaceActionType::Button
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
					FText::Format(LOCTEXT("ContextMenu_Columns_CopySingleColumn", "Copy {0}(s)"), Column.GetTitleName()),
					FText::Format(LOCTEXT("ContextMenu_Columns_CopySingleColumn_Desc", "Copy {0}(s) of selected file to clipboard"), Column.GetTitleName()),
					FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Copy"), Action_CopyColumn, NAME_None, EUserInterfaceActionType::Button
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

TSharedRef<SWidget> SPakFileView::OnBuildClassFilterMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	if (ClassFilterMap.Num() > 0)
	{
		MenuBuilder.BeginSection("FileViewShowAllClasses", LOCTEXT("QuickFilterHeading", "Quick Filter"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ShowAllClasses", "Show/Hide All"),
				LOCTEXT("ShowAllClasses_Tooltip", "Change filtering to show/hide all classes"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SPakFileView::OnShowAllClassesExecute),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SPakFileView::IsShowAllClassesChecked)),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection("FileViewClassesEntries", LOCTEXT("ClassesHeading", "Classes"));
	for (const auto& Pair : ClassFilterMap)
	{
		const FString ClassString = Pair.Key.ToString();
		const FText ClassText(FText::AsCultureInvariant(ClassString));

		const TSharedRef<SWidget> TextBlock = SNew(STextBlock)
			.Text(ClassText)
			.ShadowColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f))
			.ShadowOffset(FVector2D(1.0f, 1.0f))
			.ColorAndOpacity(FSlateColor(FClassColumn::GetColorByClass(*ClassString)));

		MenuBuilder.AddMenuEntry(
			FUIAction(FExecuteAction::CreateSP(this, &SPakFileView::OnToggleClassesExecute, Pair.Key),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SPakFileView::IsClassesFilterChecked, Pair.Key)),
			TextBlock,
			NAME_None,
			FText::Format(LOCTEXT("Class_Tooltip", "Filter the File View to show/hide class: {0}"), ClassText),
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SPakFileView::OnBuildPakFilterMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);

	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	if (PakAnalyzer)
	{
		if (PakFilterMap.Num() > 0)
		{
			MenuBuilder.BeginSection("FileViewShowAllPaks", LOCTEXT("QuickFilterHeading", "Quick Filter"));
			{
				MenuBuilder.AddMenuEntry(
					LOCTEXT("ShowAllPaks", "Show/Hide All"),
					LOCTEXT("ShowAllPaks_Tooltip", "Change filtering to show/hide all paks"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(this, &SPakFileView::OnShowAllPaksExecute),
						FCanExecuteAction(),
						FIsActionChecked::CreateSP(this, &SPakFileView::IsShowAllPaksChecked)),
					NAME_None,
					EUserInterfaceActionType::ToggleButton
				);
			}
			MenuBuilder.EndSection();
		}

		MenuBuilder.BeginSection("FileViewPaksEntries", LOCTEXT("PaksHeading", "Paks"));
		for (int32 i = 0; i < PakFilterMap.Num(); ++i)
		{
			const FPakFilterInfo& Info = PakFilterMap[i];
			const FString PakString = Info.PakName.ToString();
			const FText PakText(FText::AsCultureInvariant(PakString));

			const TSharedRef<SWidget> TextBlock = SNew(STextBlock)
				.Text(PakText)
				.ShadowColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ColorAndOpacity(FSlateColor(FClassColumn::GetColorByClass(*PakString)));

			MenuBuilder.AddMenuEntry(
				FUIAction(FExecuteAction::CreateSP(this, &SPakFileView::OnTogglePakExecute, i),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SPakFileView::IsPakFilterChecked, i)),
				TextBlock,
				NAME_None,
				FText::Format(LOCTEXT("Pak_Tooltip", "Filter the File View to show/hide pak: {0}"), PakText),
				EUserInterfaceActionType::ToggleButton
			);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void SPakFileView::OnShowAllClassesExecute()
{
	const bool bIsShowAll = IsShowAllClassesChecked();
	for (auto& Pair : ClassFilterMap)
	{
		Pair.Value = !bIsShowAll;
	}

	MarkDirty(true);
}

bool SPakFileView::IsShowAllClassesChecked() const
{
	for (const auto& Pair : ClassFilterMap)
	{
		if (!Pair.Value)
		{
			return false;
		}
	}

	return true;
}

void SPakFileView::OnToggleClassesExecute(FName InClassName)
{
	bool* bShow = ClassFilterMap.Find(InClassName);
	if (bShow)
	{
		*bShow = !(*bShow);
		MarkDirty(true);
	}
}

bool SPakFileView::IsClassesFilterChecked(FName InClassName) const
{
	const bool* bShow = ClassFilterMap.Find(InClassName);
	return bShow ? *bShow : false;
}

void SPakFileView::FillClassesFilter()
{
	ClassFilterMap.Empty();
	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	if (PakAnalyzer)
	{
		const TArray<FPakTreeEntryPtr>& TreeRoots = PakAnalyzer->GetPakTreeRootNode();
		for (const FPakTreeEntryPtr& TreeRoot : TreeRoots)
		{
			for (const auto& Pair : TreeRoot->FileClassMap)
			{
				ClassFilterMap.Add(Pair.Key, true);
			}
		}
	}
}

void SPakFileView::OnShowAllPaksExecute()
{
	const bool bIsShowAll = IsShowAllPaksChecked();
	for (FPakFilterInfo& Info : PakFilterMap)
	{
		Info.bShow = !bIsShowAll;
	}

	MarkDirty(true);
}

bool SPakFileView::IsShowAllPaksChecked() const
{
	for (const FPakFilterInfo& Info : PakFilterMap)
	{
		if (!Info.bShow)
		{
			return false;
		}
	}

	return true;
}

void SPakFileView::OnTogglePakExecute(int32 InPakIndex)
{
	if (PakFilterMap.IsValidIndex(InPakIndex))
	{
		PakFilterMap[InPakIndex].bShow = !PakFilterMap[InPakIndex].bShow;
		MarkDirty(true);
	}
}

bool SPakFileView::IsPakFilterChecked(int32 InPakIndex) const
{
	if (PakFilterMap.IsValidIndex(InPakIndex))
	{
		return PakFilterMap[InPakIndex].bShow;
	}

	return false;
}

void SPakFileView::FillPaksFilter()
{
	PakFilterMap.Empty();

	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	if (PakAnalyzer)
	{
		const TArray<FPakFileSumaryPtr>& Summaries = PakAnalyzer->GetPakFileSumary();
		PakFilterMap.AddZeroed(Summaries.Num());

		for (int32 i = 0; i < Summaries.Num(); ++i)
		{
			FPakFilterInfo Info;
			Info.bShow = true;
			Info.PakName = *FPaths::GetCleanFilename(Summaries[i]->PakFilePath);

			PakFilterMap[i] = Info;
		}
	}
}

void SPakFileView::InitializeAndShowHeaderColumns()
{
	FileColumns.Empty();

	// Name Column
	FFileColumn& NameColumn = FileColumns.Emplace(FFileColumn::NameColumnName, FFileColumn(0, FFileColumn::NameColumnName, LOCTEXT("NameColumn", "Name"), LOCTEXT("NameColumnTip", "File name"), 2.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered));
	NameColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->Filename.LexicalLess(B->Filename);
		}
	);
	NameColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return B->Filename.LexicalLess(A->Filename);
		}
	);

	// Path Column
	FFileColumn& PathColumn = FileColumns.Emplace(FFileColumn::PathColumnName, FFileColumn(1, FFileColumn::PathColumnName, LOCTEXT("PathColumn", "Path"), LOCTEXT("PathColumnTip", "File path in pak"), 3.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	PathColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->Path < B->Path;
		}
	);
	PathColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return B->Path < A->Path;
		}
	);

	// Class Column
	FFileColumn& ClassColumn = FileColumns.Emplace(FFileColumn::ClassColumnName, FFileColumn(2, FFileColumn::ClassColumnName, LOCTEXT("ClassColumn", "Class"), LOCTEXT("ClassColumnTip", "Class name in asset registry or file extension if not found"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	ClassColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->Class.LexicalLess(B->Class);
		}
	);
	ClassColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return B->Class.LexicalLess(A->Class);
		}
	);

	// Dependency Count Column
	FFileColumn& DependencyCountColumn = FileColumns.Emplace(FFileColumn::DependencyCountColumnName, FFileColumn(3, FFileColumn::DependencyCountColumnName, LOCTEXT("DependencyCountColumn", "Dependency Count"), LOCTEXT("DependencyCountColumnTip", "Packages this package depends on"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	DependencyCountColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			const int32 ACount = A->AssetSummary.IsValid() ? A->AssetSummary->DependencyList.Num() : 0;
			const int32 BCount = B->AssetSummary.IsValid() ? B->AssetSummary->DependencyList.Num() : 0;
			return ACount < BCount;
		}
	);
	DependencyCountColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			const int32 ACount = A->AssetSummary.IsValid() ? A->AssetSummary->DependencyList.Num() : 0;
			const int32 BCount = B->AssetSummary.IsValid() ? B->AssetSummary->DependencyList.Num() : 0;
			return BCount < ACount;
		}
	);

	// Dependent Count Column
	FFileColumn& DependentCountColumn = FileColumns.Emplace(FFileColumn::DependentCountColumnName, FFileColumn(4, FFileColumn::DependentCountColumnName, LOCTEXT("DependentCountColumn", "Dependent Count"), LOCTEXT("DependentCountColumnTip", "Packages depend on this package"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	DependentCountColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			const int32 ACount = A->AssetSummary.IsValid() ? A->AssetSummary->DependentList.Num() : 0;
			const int32 BCount = B->AssetSummary.IsValid() ? B->AssetSummary->DependentList.Num() : 0;
			return ACount < BCount;
		}
	);
	DependentCountColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			const int32 ACount = A->AssetSummary.IsValid() ? A->AssetSummary->DependentList.Num() : 0;
			const int32 BCount = B->AssetSummary.IsValid() ? B->AssetSummary->DependentList.Num() : 0;
			return BCount < ACount;
		}
	);

	// Offset Column
	FFileColumn& OffsetColumn = FileColumns.Emplace(FFileColumn::OffsetColumnName, FFileColumn(5, FFileColumn::OffsetColumnName, LOCTEXT("OffsetColumn", "Offset"), LOCTEXT("OffsetColumnTip", "File offset in pak"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	OffsetColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->PakEntry.Offset < B->PakEntry.Offset;
		}
	);
	OffsetColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return B->PakEntry.Offset < A->PakEntry.Offset;
		}
	);

	// Size Column
	FFileColumn& SizeColumn = FileColumns.Emplace(FFileColumn::SizeColumnName, FFileColumn(6, FFileColumn::SizeColumnName, LOCTEXT("SizeColumn", "Size"), LOCTEXT("SizeColumnTip", "File original size"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	SizeColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->PakEntry.UncompressedSize < B->PakEntry.UncompressedSize;
		}
	);
	SizeColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return B->PakEntry.UncompressedSize < A->PakEntry.UncompressedSize;
		}
	);
	
	// Compressed Size Column
	FFileColumn& CompressedSizeColumn = FileColumns.Emplace(FFileColumn::CompressedSizeColumnName, FFileColumn(7, FFileColumn::CompressedSizeColumnName, LOCTEXT("CompressedSizeColumn", "Compressed Size"), LOCTEXT("CompressedSizeColumnTip", "File compressed size"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	CompressedSizeColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->PakEntry.Size < B->PakEntry.Size;
		}
	);
	CompressedSizeColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return B->PakEntry.Size < A->PakEntry.Size;
		}
	);
	
	// Compressed Block Count
	FFileColumn& CompressionBlockCountColumn = FileColumns.Emplace(FFileColumn::CompressionBlockCountColumnName, FFileColumn(8, FFileColumn::CompressionBlockCountColumnName, LOCTEXT("CompressionBlockCountColumn", "Compression Block Count"), LOCTEXT("CompressionBlockCountColumnTip", "File compression block count"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeFiltered | EFileColumnFlags::CanBeHidden));
	CompressionBlockCountColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->PakEntry.CompressionBlocks.Num() < B->PakEntry.CompressionBlocks.Num();
		}
	);
	CompressionBlockCountColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return B->PakEntry.CompressionBlocks.Num() < A->PakEntry.CompressionBlocks.Num();
		}
	);
	
	// Compressed Block Size
	FileColumns.Emplace(FFileColumn::CompressionBlockSizeColumnName, FFileColumn(9, FFileColumn::CompressionBlockSizeColumnName, LOCTEXT("CompressionBlockSizeColumn", "Compression Block Size"), LOCTEXT("CompressionBlockSizeColumnTip", "File compression block size"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeHidden));
	
	// Compression Method
	FFileColumn& CompressionMethodColumn = FileColumns.Emplace(FFileColumn::CompressionMethodColumnName, FFileColumn(10, FFileColumn::CompressionMethodColumnName, LOCTEXT("CompressionMethod", "Compression Method"), LOCTEXT("CompressionMethodTip", "Compression method name used to compress this file"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeHidden));
	CompressionMethodColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->CompressionMethod.LexicalLess(B->CompressionMethod);
		}
	);
	CompressionMethodColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return B->CompressionMethod.LexicalLess(A->CompressionMethod);
		}
	);
	
	// Owner Pak
	FFileColumn& OwnerPakColumn = FileColumns.Emplace(FFileColumn::OwnerPakColumnName, FFileColumn(11, FFileColumn::OwnerPakColumnName, LOCTEXT("OwnerPakColumn", "Onwer Pak"), LOCTEXT("OnwerPakColumnTip", "Owner Pak Name"), 2.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeHidden | EFileColumnFlags::CanBeFiltered));
	OwnerPakColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->OwnerPakIndex < B->OwnerPakIndex;
		}
	);
	OwnerPakColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return B->OwnerPakIndex < A->OwnerPakIndex;
		}
	);

	// SHA1
	FileColumns.Emplace(FFileColumn::SHA1ColumnName, FFileColumn(12, FFileColumn::SHA1ColumnName, LOCTEXT("SHA1Column", "SHA1"), LOCTEXT("SHA1ColumnTip", "File sha1"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeHidden));
	
	// IsEncrypted
	FFileColumn& IsEncryptedColumn = FileColumns.Emplace(FFileColumn::IsEncryptedColumnName, FFileColumn(13, FFileColumn::IsEncryptedColumnName, LOCTEXT("IsEncryptedColumn", "IsEncrypted"), LOCTEXT("IsEncryptedColumnTip", "Is file encrypted in pak?"), 1.f, EFileColumnFlags::ShouldBeVisible | EFileColumnFlags::CanBeHidden));
	IsEncryptedColumn.SetAscendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return A->PakEntry.IsEncrypted() < B->PakEntry.IsEncrypted();
		}
	);
	IsEncryptedColumn.SetDescendingCompareDelegate(
		[](const FPakFileEntryPtr& A, const FPakFileEntryPtr& B) -> bool
		{
			return B->PakEntry.IsEncrypted() < A->PakEntry.IsEncrypted();
		}
	);

	// Show columns.
	for (const auto& ColumnPair : FileColumns)
	{
		if (ColumnPair.Value.ShouldBeVisible())
		{
			ShowColumn(ColumnPair.Key);
		}
	}
}

const FFileColumn* SPakFileView::FindCoulum(const FName ColumnId) const
{
	return FileColumns.Find(ColumnId);
}

FFileColumn* SPakFileView::FindCoulum(const FName ColumnId)
{
	return FileColumns.Find(ColumnId);
}

FText SPakFileView::GetSearchText() const
{
	return SearchBox.IsValid() ? SearchBox->GetText() : FText();
}

EColumnSortMode::Type SPakFileView::GetSortModeForColumn(const FName ColumnId) const
{
	if (CurrentSortedColumn != ColumnId)
	{
		return EColumnSortMode::None;
	}

	return CurrentSortMode;
}

void SPakFileView::OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	FFileColumn* Column = FindCoulum(ColumnId);
	if (!Column)
	{
		return;
	}

	if (!Column->CanBeSorted())
	{
		return;
	}

	CurrentSortedColumn = ColumnId;
	CurrentSortMode = SortMode;
	MarkDirty(true);
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
		//.ManualWidth(Column->GetInitialWidth())
		.FillWidth(Column->GetFillWidth())
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

bool SPakFileView::HasOneFileSelected() const
{
	TArray<FPakFileEntryPtr> SelectedItems;
	GetSelectedItems(SelectedItems);

	return SelectedItems.Num() == 1;
}

bool SPakFileView::HasFileSelected() const
{
	TArray<FPakFileEntryPtr> SelectedItems;
	GetSelectedItems(SelectedItems);

	return SelectedItems.Num() > 0;
}

void SPakFileView::OnCopyAllColumnsExecute()
{
	FString Value;

	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	TArray<FPakFileEntryPtr> SelectedItems;
	GetSelectedItems(SelectedItems);

	if (SelectedItems.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> FileObjects;

		for (const FPakFileEntryPtr PakFileItem : SelectedItems)
		{
			if (PakFileItem.IsValid())
			{
				const FPakEntry* PakEntry = &PakFileItem->PakEntry;
				TSharedRef<FJsonObject> FileObject = MakeShareable(new FJsonObject);

				FileObject->SetStringField(TEXT("Name"), PakFileItem->Filename.ToString());
				FileObject->SetStringField(TEXT("Path"), PakFileItem->Path);
				FileObject->SetNumberField(TEXT("Offset"), PakEntry->Offset);
				FileObject->SetNumberField(TEXT("Size"), PakEntry->UncompressedSize);
				FileObject->SetNumberField(TEXT("Compressed Size"), PakEntry->Size);
				FileObject->SetNumberField(TEXT("Compressed Block Count"), PakEntry->CompressionBlocks.Num());
				FileObject->SetNumberField(TEXT("Compressed Block Size"), PakEntry->CompressionBlockSize);
				FileObject->SetStringField(TEXT("Compression Method"), PakFileItem->CompressionMethod.ToString());
				FileObject->SetStringField(TEXT("SHA1"), BytesToHex(PakEntry->Hash, sizeof(PakEntry->Hash)));
				FileObject->SetStringField(TEXT("IsEncrypted"), PakEntry->IsEncrypted() ? TEXT("True") : TEXT("False"));
				FileObject->SetStringField(TEXT("Class"), PakFileItem->Class.ToString());
				FileObject->SetNumberField(TEXT("Dependency Count"), PakFileItem->AssetSummary.IsValid() ? PakFileItem->AssetSummary->DependencyList.Num() : 0);
				FileObject->SetNumberField(TEXT("Dependent Count"), PakFileItem->AssetSummary.IsValid() ? PakFileItem->AssetSummary->DependentList.Num() : 0);
				FileObject->SetStringField(TEXT("OwnerPak"), PakAnalyzer && PakAnalyzer->GetPakFileSumary().IsValidIndex(PakFileItem->OwnerPakIndex) ? FPaths::GetCleanFilename(PakAnalyzer->GetPakFileSumary()[PakFileItem->OwnerPakIndex]->PakFilePath) : TEXT(""));

				FileObjects.Add(MakeShareable(new FJsonValueObject(FileObject)));
			}
		}

		TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&Value);
		FJsonSerializer::Serialize(FileObjects, JsonWriter);
		JsonWriter->Close();
	}

	FPlatformApplicationMisc::ClipboardCopy(*Value);
}

void SPakFileView::OnCopyColumnExecute(const FName ColumnId)
{
	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	TArray<FString> Values;
	TArray<FPakFileEntryPtr> SelectedItems;
	GetSelectedItems(SelectedItems);

	for (const FPakFileEntryPtr PakFileItem : SelectedItems)
	{
		if (PakFileItem.IsValid())
		{
			const FPakEntry* PakEntry = &PakFileItem->PakEntry;
			if (ColumnId == FFileColumn::NameColumnName)
			{
				Values.Add(PakFileItem->Filename.ToString());
			}
			else if (ColumnId == FFileColumn::PathColumnName)
			{
				Values.Add(PakFileItem->Path);
			}
			else if (ColumnId == FFileColumn::ClassColumnName)
			{
				Values.Add(PakFileItem->Class.ToString());
			}
			else if (ColumnId == FFileColumn::OffsetColumnName)
			{
				Values.Add(FString::Printf(TEXT("%lld"), PakEntry->Offset));
			}
			else if (ColumnId == FFileColumn::SizeColumnName)
			{
				Values.Add(FString::Printf(TEXT("%lld"), PakEntry->UncompressedSize));
			}
			else if (ColumnId == FFileColumn::CompressedSizeColumnName)
			{
				Values.Add(FString::Printf(TEXT("%lld"), PakEntry->Size));
			}
			else if (ColumnId == FFileColumn::CompressionBlockCountColumnName)
			{
				Values.Add(FString::Printf(TEXT("%d"), PakEntry->CompressionBlocks.Num()));
			}
			else if (ColumnId == FFileColumn::CompressionBlockSizeColumnName)
			{
				Values.Add(FString::Printf(TEXT("%u"), PakEntry->CompressionBlockSize));
			}
			else if (ColumnId == FFileColumn::CompressionMethodColumnName)
			{
				Values.Add(PakFileItem->CompressionMethod.ToString());
			}
			else if (ColumnId == FFileColumn::SHA1ColumnName)
			{
				Values.Add(BytesToHex(PakEntry->Hash, sizeof(PakEntry->Hash)));
			}
			else if (ColumnId == FFileColumn::IsEncryptedColumnName)
			{
				Values.Add(FString::Printf(TEXT("%s"), PakEntry->IsEncrypted() ? TEXT("True") : TEXT("False")));
			}
			else if (ColumnId == FFileColumn::DependencyCountColumnName)
			{
				Values.Add(FString::Printf(TEXT("%d"), PakFileItem->AssetSummary.IsValid() ? PakFileItem->AssetSummary->DependencyList.Num() : 0));
			}
			else if (ColumnId == FFileColumn::DependentCountColumnName)
			{
				Values.Add(FString::Printf(TEXT("%d"), PakFileItem->AssetSummary.IsValid() ? PakFileItem->AssetSummary->DependentList.Num() : 0));
			}
			else if (ColumnId == FFileColumn::OwnerPakColumnName)
			{
				Values.Add(FString::Printf(TEXT("%s"), PakAnalyzer && PakAnalyzer->GetPakFileSumary().IsValidIndex(PakFileItem->OwnerPakIndex) ? *FPaths::GetCleanFilename(PakAnalyzer->GetPakFileSumary()[PakFileItem->OwnerPakIndex]->PakFilePath) : TEXT("")));
			}
		}
	}

	FPlatformApplicationMisc::ClipboardCopy(*FString::Join(Values, TEXT("\n")));
}

void SPakFileView::OnJumpToTreeViewExecute()
{
	TArray<FPakFileEntryPtr> SelectedItems;
	GetSelectedItems(SelectedItems);

	if (SelectedItems.Num() > 0 && SelectedItems[0].IsValid())
	{
		FWidgetDelegates::GetOnSwitchToTreeViewDelegate().Broadcast(SelectedItems[0]->Path, SelectedItems[0]->OwnerPakIndex);
	}
}

void SPakFileView::MarkDirty(bool bInIsDirty)
{
	bIsDirty = bInIsDirty;
}

void SPakFileView::OnSortAndFilterFinihed(const FName InSortedColumn, EColumnSortMode::Type InSortMode, const FString& InSearchText)
{
	FFunctionGraphTask::CreateAndDispatchWhenReady([this, InSearchText]()
		{
			IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

			InnderTask->RetriveResult(FileCache);
			FillFilesSummary();

			FileListView->RebuildList();

			if (!InSearchText.Equals(CurrentSearchText, ESearchCase::IgnoreCase))
			{
				// Search text changed during sort or filter
				MarkDirty(true);
			}
		},
		TStatId(), nullptr, ENamedThreads::GameThread);
}

FText SPakFileView::GetFileCount() const
{
	IPakAnalyzer* Analyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const int32 CurrentFileCount = FMath::Clamp(FileCache.Num() - 1, 0, FileCache.Num());

	int32 TotalFileCount = 0;
	if (Analyzer)
	{
		const TArray<FPakFileSumaryPtr>& Summaries = Analyzer->GetPakFileSumary();
		for (const FPakFileSumaryPtr& Summary : Summaries)
		{
			TotalFileCount += Summary->FileCount;
		}
	}

	return FText::Format(FTextFormat::FromString(TEXT("{0} / {1} files")), FText::AsNumber(CurrentFileCount), FText::AsNumber(TotalFileCount));
}

bool SPakFileView::IsFileListEmpty() const
{
	return FileCache.Num() - 1 <= 0;
}

void SPakFileView::OnExportToJson()
{
	bool bOpened = false;
	TArray<FString> OutFileNames;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FSlateApplication::Get().CloseToolTip();

		bOpened = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("OpenExportDialogTitleText", "Select output json file path...").ToString(),
			TEXT(""),
			TEXT(""),
			TEXT("Json Files (*.json)|*.json|All Files (*.*)|*.*"),
			EFileDialogFlags::None,
			OutFileNames);
	}

	if (!bOpened || OutFileNames.Num() <= 0)
	{
		return;
	}

	TArray<FPakFileEntryPtr> SelectedItems;
	GetSelectedItems(SelectedItems);

	IPakAnalyzerModule::Get().GetPakAnalyzer()->ExportToJson(OutFileNames[0], SelectedItems);
}

void SPakFileView::OnExportToCsv()
{
	bool bOpened = false;
	TArray<FString> OutFileNames;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FSlateApplication::Get().CloseToolTip();

		bOpened = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("OpenExportDialogTitleText", "Select output csv file path...").ToString(),
			TEXT(""),
			TEXT(""),
			TEXT("Json Files (*.csv)|*.csv|All Files (*.*)|*.*"),
			EFileDialogFlags::None,
			OutFileNames);
	}

	if (!bOpened || OutFileNames.Num() <= 0)
	{
		return;
	}

	TArray<FPakFileEntryPtr> SelectedItems;
	GetSelectedItems(SelectedItems);

	IPakAnalyzerModule::Get().GetPakAnalyzer()->ExportToCsv(OutFileNames[0], SelectedItems);
}

void SPakFileView::OnExtract()
{
	bool bOpened = false;
	FString OutputPath;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FSlateApplication::Get().CloseToolTip();

		bOpened = DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("OpenExtractDialogTitleText", "Select output path...").ToString(),
			TEXT(""),
			OutputPath);
	}

	if (!bOpened)
	{
		return;
	}

	TArray<FPakFileEntryPtr> SelectedItems;
	GetSelectedItems(SelectedItems);

	IPakAnalyzerModule::Get().GetPakAnalyzer()->ExtractFiles(OutputPath, SelectedItems);
}

void SPakFileView::ScrollToItem(const FString& InPath, int32 PakIndex)
{
	for (const FPakFileEntryPtr FileEntry : FileCache)
	{
		if (FileEntry->Path.Equals(InPath, ESearchCase::IgnoreCase) && FileEntry->OwnerPakIndex == PakIndex)
		{
			TArray<FPakFileEntryPtr> SelectArray = { FileEntry };
			FileListView->SetItemSelection(SelectArray, true, ESelectInfo::Direct);
			FileListView->RequestScrollIntoView(FileEntry);
			return;
		}
	}
}

void SPakFileView::OnLoadAssetReigstryFinished()
{
	FillClassesFilter();

	MarkDirty(true);
}

void SPakFileView::OnLoadPakFinished()
{
	FillClassesFilter();
	FillPaksFilter();

	MarkDirty(true);
}

void SPakFileView::OnParseAssetFinished()
{
	FillClassesFilter();

	MarkDirty(true);
}

void SPakFileView::FillFilesSummary()
{
	FilesSummary->PakEntry.Offset = 0;
	FilesSummary->PakEntry.UncompressedSize = 0;
	FilesSummary->PakEntry.Size = 0;

	if (FileCache.Num() > 0)
	{
		for (FPakFileEntryPtr PakFileEntryPtr : FileCache)
		{
			FilesSummary->PakEntry.UncompressedSize += PakFileEntryPtr->PakEntry.UncompressedSize;
			FilesSummary->PakEntry.Size += PakFileEntryPtr->PakEntry.Size;
		}

		FileCache.Add(FilesSummary);
	}
}

bool SPakFileView::GetSelectedItems(TArray<FPakFileEntryPtr>& OutSelectedItems) const
{
	if (FileListView.IsValid())
	{
		FileListView->GetSelectedItems(OutSelectedItems);
		OutSelectedItems.Remove(FilesSummary);
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
