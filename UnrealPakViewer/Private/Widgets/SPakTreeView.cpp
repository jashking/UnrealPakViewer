#include "SPakTreeView.h"

#include "DesktopPlatformModule.h"
#include "EditorStyle.h"
#include "Framework/Application/SlateApplication.h"
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
			.OnContextMenuOpening(this, &SPakTreeView::OnGenerateContextMenu)
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
				SAssignNew(CompressionBlockSizeRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressionBlockSize", "Compression Block Size:")).ValueText(this, &SPakTreeView::GetSelectionCompressionBlockSize).ValueToolTipText(this, &SPakTreeView::GetSelectionCompressionBlockSizeToolTip)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SAssignNew(CompressionMethodRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressionMethod", "Compression Method:")).ValueText(this, &SPakTreeView::GetCompressionMethod)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SAssignNew(SHA1Row, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_SHA1", "SHA1:")).ValueText(this, &SPakTreeView::GetSelectionSHA1)
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
	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	if (PakAnalyzer && PakAnalyzer->IsLoadDirty(LastLoadGuid))
	{
		LastLoadGuid = PakAnalyzer->GetLastLoadGuid();

		TreeNodes.Empty();
		TreeNodes.Add(PakAnalyzer->GetPakTreeRootNode());

		if (TreeView.IsValid())
		{
			TreeView->RequestTreeRefresh();
		}
	}

	if (!DelayExpandItemPath.IsEmpty())
	{
		ExpandTreeItem(DelayExpandItemPath);
		DelayExpandItemPath = TEXT("");
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
					SNew(STextBlock).Text(FText::FromName(TreeNode->Filename)).ColorAndOpacity(FLinearColor::Green)
				]

				+ SHorizontalBox::Slot()
				//.MaxWidth(100.f)
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
					.MinDesiredWidth(150.f)
					.MaxDesiredWidth(200.f)
					.ToolTipText(FText::Format(LOCTEXT("Tree_View_CompressedPercent", "{0}'s compressed size percent of total compressed size"), FText::FromName(TreeNode->Filename)))
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
				SNew(STextBlock).Text(FText::FromName(TreeNode->Filename)).ColorAndOpacity(FLinearColor::White)
			];
	}
}

void SPakTreeView::OnGetTreeNodeChildren(FPakTreeEntryPtr InParent, TArray<FPakTreeEntryPtr>& OutChildren)
{
	if (InParent.IsValid() && InParent->bIsDirectory)
	{
		OutChildren.Empty();
		
		for (auto& Pair : InParent->ChildrenMap)
		{
			FPakTreeEntryPtr Child = Pair.Value;
			OutChildren.Add(Child);
		}
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
	CompressionMethodRow->SetVisibility(bIsSelectionFile ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	SHA1Row->SetVisibility(bIsSelectionFile ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	IsEncryptedRow->SetVisibility(bIsSelectionFile ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);

	FileCountRow->SetVisibility(bIsSelectionDirectory ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
}

void SPakTreeView::ExpandTreeItem(const FString& InPath)
{
	static const TCHAR* Delims[2] = { TEXT("\\"), TEXT("/") };

	TArray<FString> PathItems;
	InPath.ParseIntoArray(PathItems, Delims, 2);

	if (PathItems.Num() <= 0 || TreeNodes.Num() <= 0)
	{
		return;
	}

	TreeView->ClearExpandedItems();
	TreeView->ClearSelection();

	FPakTreeEntryPtr Parent = TreeNodes[0];
	for (int32 i = 0; i < PathItems.Num(); ++i)
	{
		FPakTreeEntryPtr* Child = Parent->ChildrenMap.Find(*PathItems[i]);
		if (Child)
		{
			TreeView->SetItemExpansion(Parent, true);
			if (i == PathItems.Num() - 1)
			{
				TreeView->SetItemSelection(*Child, true, ESelectInfo::Direct);
				TreeView->RequestScrollIntoView(*Child);
			}

			Parent = *Child;
			continue;
		}
		else
		{
			break;
		}
	}
}

FORCEINLINE FText SPakTreeView::GetSelectionName() const
{
	return CurrentSelectedItem.IsValid() ? FText::FromName(CurrentSelectedItem->Filename) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionPath() const
{
	return CurrentSelectedItem.IsValid() ? FText::FromString(CurrentSelectedItem->Path) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionOffset() const
{
	const FPakEntry* PakEntry = CurrentSelectedItem.IsValid() ? &CurrentSelectedItem->PakEntry : nullptr;
	return PakEntry ? FText::AsNumber(PakEntry->Offset) : FText();
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
	const FPakEntry* PakEntry = CurrentSelectedItem.IsValid() ? &CurrentSelectedItem->PakEntry : nullptr;
	return PakEntry ? FText::AsNumber(PakEntry->CompressionBlocks.Num()) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionCompressionBlockSize() const
{
	const FPakEntry* PakEntry = CurrentSelectedItem.IsValid() ? &CurrentSelectedItem->PakEntry : nullptr;
	return PakEntry ? FText::AsMemory(PakEntry->CompressionBlockSize, EMemoryUnitStandard::IEC) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionCompressionBlockSizeToolTip() const
{
	const FPakEntry* PakEntry = CurrentSelectedItem.IsValid() ? &CurrentSelectedItem->PakEntry : nullptr;
	return PakEntry ? FText::AsNumber(PakEntry->CompressionBlockSize) : FText();
}

FORCEINLINE FText SPakTreeView::GetCompressionMethod() const
{
	const FPakEntry* PakEntry = CurrentSelectedItem.IsValid() ? &CurrentSelectedItem->PakEntry : nullptr;

	return PakEntry ? FText::FromName(CurrentSelectedItem->CompressionMethod) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionSHA1() const
{
	const FPakEntry* PakEntry = CurrentSelectedItem.IsValid() ? &CurrentSelectedItem->PakEntry : nullptr;
	return PakEntry ? FText::FromString(BytesToHex(PakEntry->Hash, sizeof(PakEntry->Hash))) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionIsEncrypted() const
{
	const FPakEntry* PakEntry = CurrentSelectedItem.IsValid() ? &CurrentSelectedItem->PakEntry : nullptr;
	return PakEntry ? FText::FromString(PakEntry->IsEncrypted() ? TEXT("True") : TEXT("False")) : FText();
}

FORCEINLINE FText SPakTreeView::GetSelectionFileCount() const
{
	return CurrentSelectedItem.IsValid() && CurrentSelectedItem->bIsDirectory ? FText::AsNumber(CurrentSelectedItem->FileCount) : FText();
}

TSharedPtr<SWidget> SPakTreeView::OnGenerateContextMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	// Extract menu
	MenuBuilder.BeginSection("Extract", LOCTEXT("ContextMenu_Header_Extract", "Extract"));
	{
		FUIAction Action_Extract
		(
			FExecuteAction::CreateSP(this, &SPakTreeView::OnExtractExecute),
			FCanExecuteAction::CreateSP(this, &SPakTreeView::HasSelection)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_Extract", "Extract..."),
			LOCTEXT("ContextMenu_Extract_Desc", "Extract current selected file or folder to disk"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ResetColumn"), Action_Extract, NAME_None, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	// Extract menu
	MenuBuilder.BeginSection("Operation", LOCTEXT("ContextMenu_Header_Operation", "Operation"));
	{
		FUIAction Action_JumpToFileView
		(
			FExecuteAction::CreateSP(this, &SPakTreeView::OnJumpToFileViewExecute),
			FCanExecuteAction::CreateSP(this, &SPakTreeView::HasSelection)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_JumpToFileView", "Jump To File View"),
			LOCTEXT("ContextMenu_JumpToFileView_Desc", "Show current selected file in file view"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "Profiler.EventGraph.ResetColumn"), Action_JumpToFileView, NAME_None, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SPakTreeView::OnExtractExecute()
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

	TArray<FPakFileEntryPtr> TargetFiles;

	TArray<FPakTreeEntryPtr> SelectedItems;
	TreeView->GetSelectedItems(SelectedItems);
	for (FPakTreeEntryPtr PakTreeEntry : SelectedItems)
	{
		RetriveFiles(PakTreeEntry, TargetFiles);
	}

	IPakAnalyzerModule::Get().GetPakAnalyzer()->ExtractFiles(OutputPath, TargetFiles);
}

void SPakTreeView::OnJumpToFileViewExecute()
{

}

bool SPakTreeView::HasSelection() const
{
	TArray<FPakTreeEntryPtr> SelectedItems;
	TreeView->GetSelectedItems(SelectedItems);

	return SelectedItems.Num() > 0;
}

void SPakTreeView::RetriveFiles(FPakTreeEntryPtr InRoot, TArray<FPakFileEntryPtr>& OutFiles)
{
	if (InRoot->bIsDirectory)
	{
		for (auto& Pair : InRoot->ChildrenMap)
		{
			FPakTreeEntryPtr Child = Pair.Value;
			if (Child->bIsDirectory)
			{
				RetriveFiles(Child, OutFiles);
			}
			else
			{
				OutFiles.Add(StaticCastSharedPtr<FPakFileEntry>(Child));
			}
		}
	}
	else
	{
		OutFiles.Add(StaticCastSharedPtr<FPakFileEntry>(InRoot));
	}
}

#undef LOCTEXT_NAMESPACE
