#include "SPakTreeView.h"

#include "EditorStyle.h"
#include "IPlatformFilePak.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SOverlay.h"

#include "PakAnalyzerModule.h"
#include "SKeyValueRow.h"

#define LOCTEXT_NAMESPACE "SPakTreeView"

SPakTreeView::SPakTreeView()
{

}

SPakTreeView::~SPakTreeView()
{

}

void SPakTreeView::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(2.0f)
		[
			SAssignNew(TreeView, STreeView<FPakTreeEntryPtr>)
			.SelectionMode(ESelectionMode::Single)
			.ItemHeight(12.0f)
			.TreeItemsSource(&TreeNodes)
			.OnGetChildren(this, &SPakTreeView::OnGetTreeNodeChildren)
			.OnGenerateRow(this, &SPakTreeView::OnGenerateTreeRow)
			.OnSelectionChanged(this, &SPakTreeView::OnSelectionChanged)
			//.OnContextMenuOpening(this, &SUnrealPakViewer::OnContextMenuOpening)
			//.ClearSelectionOnClick(false)
			//.OnMouseButtonDoubleClick(this, &SUnrealPakViewer::OnTreeItemDoubleClicked)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(2.0f)
		[
			SAssignNew(KeyValueBox, SVerticalBox).Visibility(EVisibility::Collapsed)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_Name", "Name:")).ValueText(this, &SPakTreeView::GetSelectionName)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_Path", "Path:")).ValueText(this, &SPakTreeView::GetSelectionPath)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SAssignNew(OffsetRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_Offset", "Offset:")).ValueText(this, &SPakTreeView::GetSelectionOffset)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_Size", "Size:")).ValueText(this, &SPakTreeView::GetSelectionSize).ValueToolTipText(this, &SPakTreeView::GetSelectionSizeToolTip)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressedSize", "Compressed Size:")).ValueText(this, &SPakTreeView::GetSelectionCompressedSize).ValueToolTipText(this, &SPakTreeView::GetSelectionCompressedSizeToolTip)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressedSizeOfTotal", "Compressed Size Of Total:")).ValueText(this, &SPakTreeView::GetSelectionCompressedSizePercentOfTotal)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressedSizeOfParent", "Compressed Size Of Parent:")).ValueText(this, &SPakTreeView::GetSelectionCompressedSizePercentOfParent)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SAssignNew(CompressionBlockCountRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressionBlockCount", "Compression Block Count:")).ValueText(this, &SPakTreeView::GetSelectionCompressionBlockCount)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SAssignNew(CompressionBlockSizeRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressionBlockSize", "Compression Block Size:")).ValueText(this, &SPakTreeView::GetSelectionCompressionBlockSize)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SAssignNew(SHA1SizeRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_SHA1", "SHA1:")).ValueText(this, &SPakTreeView::GetSelectionSHA1)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SAssignNew(IsEncryptedRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_IsEncrypted", "IsEncrypted:")).ValueText(this, &SPakTreeView::GetSelectionIsEncrypted)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SAssignNew(FileCountRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_FileCount", "File Count:")).ValueText(this, &SPakTreeView::GetSelectionFileCount)
			]
		]
	];

	LastLoadGuid = LexToString(FGuid());
}

void SPakTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	if (PakAnalyzer.IsValid() && PakAnalyzer->IsLoadDirty(LastLoadGuid))
	{
		LastLoadGuid = PakAnalyzer->GetLastLoadGuid();

		TreeNodes.Empty();
		TreeNodes.Add(PakAnalyzer->GetPakTreeRootNode());

		if (TreeView.IsValid())
		{
			TreeView->RequestTreeRefresh();
		}
	}

	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

TSharedRef<ITableRow> SPakTreeView::OnGenerateTreeRow(FPakTreeEntryPtr TreeNode, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (TreeNode->bIsDirectory)
	{
		return SNew(STableRow<TSharedPtr<FPakTreeEntryPtr>>, OwnerTable)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				//.AutoWidth()
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock).Text(FText::FromString(TreeNode->Filename)).ColorAndOpacity(FLinearColor::Green)
				]

				+ SHorizontalBox::Slot()
				//.MaxWidth(100.f)
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
					.MinDesiredWidth(150.f)
					.MaxDesiredWidth(200.f)
					.ToolTipText(FText::Format(LOCTEXT("Tree_View_CompressedPercent", "{0}'s compressed size percent of total compressed size"), FText::FromString(TreeNode->Filename)))
					[
						SNew(SOverlay)

						+ SOverlay::Slot()
						[
							SNew(SProgressBar).Percent(TreeNode->CompressedSizePercentOfTotal)
						]

						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%.2f%%"), TreeNode->CompressedSizePercentOfTotal * 100)))
							.ColorAndOpacity(FLinearColor::Black)
						]
					]
				]
			];
	}
	else
	{
		return SNew(STableRow<TSharedPtr<FPakTreeEntryPtr>>, OwnerTable)
			[
				SNew(STextBlock).Text(FText::FromString(TreeNode->Filename)).ColorAndOpacity(FLinearColor::White)
			];
	}
}

void SPakTreeView::OnGetTreeNodeChildren(FPakTreeEntryPtr InParent, TArray<FPakTreeEntryPtr>& OutChildren)
{
	if (InParent.IsValid() && InParent->bIsDirectory)
	{
		OutChildren.Empty();
		OutChildren.Append(InParent->Children);
	}
}

void SPakTreeView::OnSelectionChanged(FPakTreeEntryPtr SelectedItem, ESelectInfo::Type SelectInfo)
{
	CurrentSelectedItem = SelectedItem;

	KeyValueBox->SetVisibility(CurrentSelectedItem.IsValid() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);

	const bool bIsSelectionFile = CurrentSelectedItem.IsValid() && !CurrentSelectedItem->bIsDirectory;
	const bool bIsSelectionDirectory = CurrentSelectedItem.IsValid() && CurrentSelectedItem->bIsDirectory;

	OffsetRow->SetVisibility(bIsSelectionFile ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	CompressionBlockCountRow->SetVisibility(bIsSelectionFile ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	CompressionBlockSizeRow->SetVisibility(bIsSelectionFile ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	SHA1SizeRow->SetVisibility(bIsSelectionFile ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	IsEncryptedRow->SetVisibility(bIsSelectionFile ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);

	FileCountRow->SetVisibility(bIsSelectionDirectory ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
}

FORCEINLINE FText SPakTreeView::GetSelectionName() const
{
	return CurrentSelectedItem.IsValid() ? FText::FromString(CurrentSelectedItem->Filename) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionPath() const
{
	return CurrentSelectedItem.IsValid() ? FText::FromString(CurrentSelectedItem->Path) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionOffset() const
{
	return CurrentSelectedItem.IsValid() && !CurrentSelectedItem->bIsDirectory ? FText::AsNumber(CurrentSelectedItem->PakEntry->Offset) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionSize() const
{
	return CurrentSelectedItem.IsValid() ? FText::AsMemory(CurrentSelectedItem->Size, EMemoryUnitStandard::IEC) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionSizeToolTip() const
{
	return CurrentSelectedItem.IsValid() ? FText::AsNumber(CurrentSelectedItem->Size) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionCompressedSize() const
{
	return CurrentSelectedItem.IsValid() ? FText::AsMemory(CurrentSelectedItem->CompressedSize, EMemoryUnitStandard::IEC) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionCompressedSizeToolTip() const
{
	return CurrentSelectedItem.IsValid() ? FText::AsNumber(CurrentSelectedItem->CompressedSize) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionCompressedSizePercentOfTotal() const
{
	return CurrentSelectedItem.IsValid() ? FText::FromString(FString::Printf(TEXT("%.4f%%"), CurrentSelectedItem->CompressedSizePercentOfTotal * 100)) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionCompressedSizePercentOfParent() const
{
	return CurrentSelectedItem.IsValid() ? FText::FromString(FString::Printf(TEXT("%.4f%%"), CurrentSelectedItem->CompressedSizePercentOfParent * 100)) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionCompressionBlockCount() const
{
	return CurrentSelectedItem.IsValid() && !CurrentSelectedItem->bIsDirectory ? FText::AsNumber(CurrentSelectedItem->PakEntry->CompressionBlocks.Num()) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionCompressionBlockSize() const
{
	return CurrentSelectedItem.IsValid() && !CurrentSelectedItem->bIsDirectory ? FText::AsMemory(CurrentSelectedItem->PakEntry->CompressionBlockSize, EMemoryUnitStandard::IEC) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionCompressionBlockSizeToolTip() const
{
	return CurrentSelectedItem.IsValid() && !CurrentSelectedItem->bIsDirectory ? FText::AsNumber(CurrentSelectedItem->PakEntry->CompressionBlockSize) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionSHA1() const
{
	return CurrentSelectedItem.IsValid() && !CurrentSelectedItem->bIsDirectory ? FText::FromString(BytesToHex(CurrentSelectedItem->PakEntry->Hash, sizeof(CurrentSelectedItem->PakEntry->Hash))) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionIsEncrypted() const
{
	return CurrentSelectedItem.IsValid() && !CurrentSelectedItem->bIsDirectory ? FText::FromString(CurrentSelectedItem->PakEntry->IsEncrypted() ? TEXT("True") : TEXT("False")) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionFileCount() const
{
	return CurrentSelectedItem.IsValid() && CurrentSelectedItem->bIsDirectory ? FText::AsNumber(CurrentSelectedItem->FileCount) : FText();
}

#undef LOCTEXT_NAMESPACE
