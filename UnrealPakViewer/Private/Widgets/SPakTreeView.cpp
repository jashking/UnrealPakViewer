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
#include "SPakClassView.h"
#include "UnrealPakViewerStyle.h"
#include "ViewModels/WidgetDelegates.h"

#define LOCTEXT_NAMESPACE "SPakTreeView"

SPakTreeView::SPakTreeView()
{
	FWidgetDelegates::GetOnLoadAssetRegistryFinishedDelegate().AddRaw(this, &SPakTreeView::OnLoadAssetReigstryFinished);
}

SPakTreeView::~SPakTreeView()
{
	FWidgetDelegates::GetOnLoadAssetRegistryFinishedDelegate().RemoveAll(this);
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
			.Padding(0.f, 2.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_Name", "Name:")).ValueText(this, &SPakTreeView::GetSelectionName)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_Path", "Path:")).ValueText(this, &SPakTreeView::GetSelectionPath)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SAssignNew(ClassRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_Class", "Class:")).ValueText(this, &SPakTreeView::GetSelectionClass)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SAssignNew(OffsetRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_Offset", "Offset:")).ValueText(this, &SPakTreeView::GetSelectionOffset)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_Size", "Size:")).ValueText(this, &SPakTreeView::GetSelectionSize).ValueToolTipText(this, &SPakTreeView::GetSelectionSizeToolTip)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressedSize", "Compressed Size:")).ValueText(this, &SPakTreeView::GetSelectionCompressedSize).ValueToolTipText(this, &SPakTreeView::GetSelectionCompressedSizeToolTip)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressedSizeOfTotal", "Compressed Size Of Total:")).ValueText(this, &SPakTreeView::GetSelectionCompressedSizePercentOfTotal)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressedSizeOfParent", "Compressed Size Of Parent:")).ValueText(this, &SPakTreeView::GetSelectionCompressedSizePercentOfParent)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SAssignNew(CompressionBlockCountRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressionBlockCount", "Compression Block Count:")).ValueText(this, &SPakTreeView::GetSelectionCompressionBlockCount)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SAssignNew(CompressionBlockSizeRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressionBlockSize", "Compression Block Size:")).ValueText(this, &SPakTreeView::GetSelectionCompressionBlockSize).ValueToolTipText(this, &SPakTreeView::GetSelectionCompressionBlockSizeToolTip)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SAssignNew(CompressionMethodRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_CompressionMethod", "Compression Method:")).ValueText(this, &SPakTreeView::GetCompressionMethod)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SAssignNew(SHA1Row, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_SHA1", "SHA1:")).ValueText(this, &SPakTreeView::GetSelectionSHA1)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SAssignNew(IsEncryptedRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_IsEncrypted", "IsEncrypted:")).ValueText(this, &SPakTreeView::GetSelectionIsEncrypted)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 2.f)
			[
				SAssignNew(FileCountRow, SKeyValueRow).KeyText(LOCTEXT("Tree_View_Selection_FileCount", "File Count:")).ValueText(this, &SPakTreeView::GetSelectionFileCount)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(0.f, 2.f)
			[
				SAssignNew(ClassView, SPakClassView)
			]
		]
	];

	LastLoadGuid = LexToString(FGuid());
}

void SPakTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	if (PakAnalyzer && (bIsDirty || PakAnalyzer->IsLoadDirty(LastLoadGuid)))
	{
		bIsDirty = false;
		LastLoadGuid = PakAnalyzer->GetLastLoadGuid();

		TreeNodes.Empty();
		TreeNodes.Add(PakAnalyzer->GetPakTreeRootNode());

		if (TreeView.IsValid())
		{
			TreeView->RequestTreeRefresh();
		}
	}

	if (!DelayHighlightItem.IsEmpty())
	{
		ExpandTreeItem(DelayHighlightItem);
		DelayHighlightItem = TEXT("");
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
				.AutoWidth()
				.Padding(FMargin(2.f, 0.f, 2.f, 0.f))
				[
					// FUnrealPakViewerStyle::Get().GetOptionalBrush("FolderClosed")
					SNew(SImage).Image(this, &SPakTreeView::GetFolderImage, TreeNode).ColorAndOpacity(FLinearColor::Green)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromName(TreeNode->Filename)).ColorAndOpacity(FLinearColor::Green).ShadowOffset(FVector2D(1.f, 1.f))
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
	ClassRow->SetVisibility(bIsSelectionFile ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);

	FileCountRow->SetVisibility(bIsSelectionDirectory ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
	ClassView->SetVisibility(bIsSelectionDirectory ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);

	if (bIsSelectionDirectory)
	{
		ClassView->Reload(CurrentSelectedItem);
	}
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

FORCEINLINE const FSlateBrush* SPakTreeView::GetFolderImage(FPakTreeEntryPtr InTreeNode) const
{
	return TreeView->IsItemExpanded(InTreeNode) ? FUnrealPakViewerStyle::Get().GetOptionalBrush("FolderOpen") : FUnrealPakViewerStyle::Get().GetOptionalBrush("FolderClosed");
}

FORCEINLINE FText SPakTreeView::GetSelectionClass() const
{
	return CurrentSelectedItem.IsValid() && !CurrentSelectedItem->bIsDirectory ? FText::FromName(CurrentSelectedItem->Class) : FText();
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
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Extract"), Action_Extract, NAME_None, EUserInterfaceActionType::Button
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
				FExecuteAction::CreateSP(this, &SPakTreeView::OnExportToJson),
				FCanExecuteAction::CreateSP(this, &SPakTreeView::HasSelection)
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
				FExecuteAction::CreateSP(this, &SPakTreeView::OnExportToCsv),
				FCanExecuteAction::CreateSP(this, &SPakTreeView::HasSelection)
			),
			NAME_None, EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	// Operation menu
	MenuBuilder.BeginSection("Operation", LOCTEXT("ContextMenu_Header_Operation", "Operation"));
	{
		FUIAction Action_JumpToFileView
		(
			FExecuteAction::CreateSP(this, &SPakTreeView::OnJumpToFileViewExecute),
			FCanExecuteAction::CreateSP(this, &SPakTreeView::HasFileSelection)
		);
		MenuBuilder.AddMenuEntry
		(
			LOCTEXT("ContextMenu_JumpToFileView", "Show In File View"),
			LOCTEXT("ContextMenu_JumpToFileView_Desc", "Show current selected file in file view"),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Find"), Action_JumpToFileView, NAME_None, EUserInterfaceActionType::Button
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

	//const FString PakFileName = FPaths::GetBaseFilename(IPakAnalyzerModule::Get().GetPakAnalyzer()->GetPakFileSumary().PakFilePath);
	
	IPakAnalyzerModule::Get().GetPakAnalyzer()->ExtractFiles(OutputPath/* / PakFileName*/, TargetFiles);
}

void SPakTreeView::OnJumpToFileViewExecute()
{
	TArray<FPakTreeEntryPtr> SelectedItems = TreeView->GetSelectedItems();
	if (SelectedItems.Num() > 0 && SelectedItems[0].IsValid())
	{
		FWidgetDelegates::GetOnSwitchToFileViewDelegate().Broadcast(SelectedItems[0]->Path);
	}
}

bool SPakTreeView::HasSelection() const
{
	TArray<FPakTreeEntryPtr> SelectedItems;
	TreeView->GetSelectedItems(SelectedItems);

	return SelectedItems.Num() > 0;
}

bool SPakTreeView::HasFileSelection() const
{
	TArray<FPakTreeEntryPtr> SelectedItems;
	TreeView->GetSelectedItems(SelectedItems);

	return SelectedItems.Num() > 0 && !SelectedItems[0]->bIsDirectory;
}

void SPakTreeView::OnExportToJson()
{
	bool bOpened = false;
	const FString PakName = FPaths::GetBaseFilename(IPakAnalyzerModule::Get().GetPakAnalyzer()->GetPakFileSumary().PakFilePath);
	TArray<FString> OutFileNames;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FSlateApplication::Get().CloseToolTip();

		bOpened = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("OpenExportDialogTitleText", "Select output json file path...").ToString(),
			TEXT(""),
			FString::Printf(TEXT("%s.json"), *PakName),
			TEXT("Json Files (*.json)|*.json|All Files (*.*)|*.*"),
			EFileDialogFlags::None,
			OutFileNames);
	}

	if (!bOpened || OutFileNames.Num() <= 0)
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

	IPakAnalyzerModule::Get().GetPakAnalyzer()->ExportToJson(OutFileNames[0], TargetFiles);
}

void SPakTreeView::OnExportToCsv()
{
	bool bOpened = false;
	const FString PakName = FPaths::GetBaseFilename(IPakAnalyzerModule::Get().GetPakAnalyzer()->GetPakFileSumary().PakFilePath);
	TArray<FString> OutFileNames;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FSlateApplication::Get().CloseToolTip();

		bOpened = DesktopPlatform->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("OpenExportDialogTitleText", "Select output csv file path...").ToString(),
			TEXT(""),
			FString::Printf(TEXT("%s.csv"), *PakName),
			TEXT("Json Files (*.csv)|*.csv|All Files (*.*)|*.*"),
			EFileDialogFlags::None,
			OutFileNames);
	}

	if (!bOpened || OutFileNames.Num() <= 0)
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

	IPakAnalyzerModule::Get().GetPakAnalyzer()->ExportToCsv(OutFileNames[0], TargetFiles);
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

void SPakTreeView::OnLoadAssetReigstryFinished()
{
	bIsDirty = true;
	//if (CurrentSelectedItem.IsValid() && CurrentSelectedItem->bIsDirectory)
	//{
	//	ClassView->Reload(CurrentSelectedItem);
	//}
}

#undef LOCTEXT_NAMESPACE
