#include "SPakTreeView.h"

#include "EditorStyle.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SScrollBar.h"

#include "PakAnalyzerModule.h"

#define LOCTEXT_NAMESPACE "SPakTreeView"

SPakTreeView::SPakTreeView()
{

}

SPakTreeView::~SPakTreeView()
{

}

void SPakTreeView::Construct(const FArguments& InArgs)
{
	//SAssignNew(ExternalScrollbar, SScrollBar).AlwaysShowScrollbar(true);

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.HAlign(HAlign_Left)
		.Padding(0.0f, 2.0f)
		[
			SAssignNew(TreeView, STreeView<FPakTreeEntryPtr>)
			.SelectionMode(ESelectionMode::Single)
			.ItemHeight(12.0f)
			.TreeItemsSource(&TreeNodes)
			.OnGetChildren(this, &SPakTreeView::OnGetTreeNodeChildren)
			.OnGenerateRow(this, &SPakTreeView::OnGenerateTreeRow)
			//.OnContextMenuOpening(this, &SUnrealPakViewer::OnContextMenuOpening)
			//.ClearSelectionOnClick(false)
			//.OnMouseButtonDoubleClick(this, &SUnrealPakViewer::OnTreeItemDoubleClicked)
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

		const FPakFileSumary& PakFileSumary = PakAnalyzer->GetPakFileSumary();

		FPakTreeEntryPtr VirtualRoot = MakeShared<FPakTreeEntry>();
		VirtualRoot->Path = PakFileSumary.MountPoint;
		VirtualRoot->Filename = FPaths::GetCleanFilename(PakAnalyzer->GetPakFilePath());
		VirtualRoot->bIsDirectory = true;

		TreeNodes.Empty();
		TreeNodes.Add(VirtualRoot);

		if (TreeView.IsValid())
		{
			TreeView->RequestTreeRefresh();
		}
	}

	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

TSharedRef<ITableRow> SPakTreeView::OnGenerateTreeRow(FPakTreeEntryPtr TreeNode, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<ITableRow> TableRow =
		SNew(STableRow<TSharedPtr<FPakTreeEntryPtr>>, OwnerTable)
		[
			SNew(STextBlock).Text(FText::FromString(TreeNode->Filename)).ColorAndOpacity(FLinearColor::Green)
		];

	return TableRow;
}

void SPakTreeView::OnGetTreeNodeChildren(FPakTreeEntryPtr InParent, TArray<FPakTreeEntryPtr>& OutChildren)
{
	if (InParent.IsValid() && InParent->bIsDirectory)
	{
		if (InParent->Children.Num() <= 0)
		{
			TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
			if (PakAnalyzer.IsValid())
			{
				PakAnalyzer->GetPakFilesInDirectory(InParent->Path, true, true, false, InParent->Children);
			}
		}

		OutChildren.Empty();
		OutChildren.Append(InParent->Children);
	}
}

#undef LOCTEXT_NAMESPACE
