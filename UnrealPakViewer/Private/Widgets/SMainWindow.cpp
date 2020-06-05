#include "SMainWindow.h"

#include "DesktopPlatformModule.h"
#include "EditorStyleSet.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Docking/SDockTab.h"

#include "PakAnalyzer.h"

#define LOCTEXT_NAMESPACE "SMainWindow"

static const FName TreeViewTabId("UnrealPakViewerTreeView");
static const FName FileViewTabId("UnrealPakViewerFileView");

SMainWindow::SMainWindow()
{

}

SMainWindow::~SMainWindow()
{

}

void SMainWindow::Construct(const FArguments& Args)
{
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab).TabRole(ETabRole::MajorTab);

	TabManager = FGlobalTabmanager::Get()->NewTabManager(DockTab);
	TSharedRef<FWorkspaceItem> AppMenuGroup = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("UnrealPakViewerMenuGroupName", "UnrealPak Viewer"));

	TabManager->RegisterTabSpawner(TreeViewTabId, FOnSpawnTab::CreateRaw(this, &SMainWindow::OnSpawnTab, TreeViewTabId))
		.SetDisplayName(LOCTEXT("TreeViewTabTitle", "Tree View"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "UnrealPakViewer.Tabs.Tools"))
		.SetGroup(AppMenuGroup);

	TabManager->RegisterTabSpawner(FileViewTabId, FOnSpawnTab::CreateRaw(this, &SMainWindow::OnSpawnTab, FileViewTabId))
		.SetDisplayName(LOCTEXT("FileViewTabTitle", "File View"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "UnrealPakViewer.Tabs.Tools"))
		.SetGroup(AppMenuGroup);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("UnrealPakViewer_v1.0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->Split
			(
				FTabManager::NewStack()
				->AddTab(TreeViewTabId, ETabState::OpenedTab)
				->AddTab(FileViewTabId, ETabState::OpenedTab)
				->SetForegroundTab(FTabId(TreeViewTabId))
			)
		);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "UnrealPak Viewer"))
		.ClientSize(FVector2D(WINDOW_WIDTH * DPIScaleFactor, WINDOW_HEIGHT * DPIScaleFactor))
		[
			SNew(SVerticalBox)
			// Menu
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeMainMenu()
			]
			// Content Area
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				TabManager->RestoreFrom(Layout, TSharedPtr<SWindow>()).ToSharedRef()
			]
		]
	);

	OnWindowClosed.BindRaw(this, &SMainWindow::OnExit);

	FPakAnalyzer::Initialize();
}

TSharedRef<SWidget> SMainWindow::MakeMainMenu()
{
	// Create & initialize main menu.
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(nullptr);
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("FileMenu", "File"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateRaw(this, &SMainWindow::FillFileMenu)
	);

	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("OptionsMenu", "Options"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateRaw(this, &SMainWindow::FillOptionsMenu)
	);

	// Create the menu bar
	TSharedRef<SWidget> MenuBarWidget = MenuBarBuilder.MakeWidget();

	return MenuBarWidget;
}

void SMainWindow::FillFileMenu(class FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Open", "Open pak..."),
		LOCTEXT("Open_ToolTip", "Open an unreal pak file."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SMainWindow::OnLoadPakFile),
			FCanExecuteAction()
		),
		NAME_None,
		EUserInterfaceActionType::Button
	);
}

void SMainWindow::FillOptionsMenu(class FMenuBuilder& MenuBuilder)
{

}

TSharedRef<SDockTab> SMainWindow::OnSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier)
{
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;

	TSharedRef<SDockTab> DockTab = SNew(SDockTab).TabRole(ETabRole::PanelTab);

	if (TabIdentifier == TreeViewTabId)
	{
	}
	else if (TabIdentifier == FileViewTabId)
	{

	}

	DockTab->SetContent(TabWidget.ToSharedRef());
	return DockTab;
}

void SMainWindow::OnExit(const TSharedRef<SWindow>& InWindow)
{
	FPakAnalyzer::Get()->Shutdown();
}

void SMainWindow::OnLoadPakFile()
{
	TArray<FString> OutFiles;
	bool bOpened = false;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FSlateApplication::Get().CloseToolTip();

		bOpened = DesktopPlatform->OpenFileDialog
		(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("LoadPak_FileDesc", "Open pak file...").ToString(),
			TEXT(""),
			TEXT(""),
			LOCTEXT("LoadPak_FileFilter", "Pak files (*.pak)|*.pak|All files (*.*)|*.*").ToString(),
			EFileDialogFlags::None,
			OutFiles
		);
	}

	if (bOpened && OutFiles.Num() > 0)
	{
		FPakAnalyzer::Get()->LoadPakFile(OutFiles[0]);
	}
}

FReply SMainWindow::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if (DragDropOp.IsValid())
	{
		if (DragDropOp->HasFiles())
		{
			// For now, only allow a single file.
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if (Files.Num() == 1)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(Files[0], true);
				if (DraggedFileExtension == TEXT(".pak"))
				{
					FPakAnalyzer::Get()->LoadPakFile(Files[0]);
					return FReply::Handled();
				}
			}
		}
	}

	return SCompoundWidget::OnDrop(MyGeometry, DragDropEvent);
}

FReply SMainWindow::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if (DragDropOp.IsValid())
	{
		if (DragDropOp->HasFiles())
		{
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if (Files.Num() == 1)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(Files[0], true);
				if (DraggedFileExtension == TEXT(".pak"))
				{
					return FReply::Handled();
				}
			}
		}
	}

	return SCompoundWidget::OnDragOver(MyGeometry, DragDropEvent);
}

#undef LOCTEXT_NAMESPACE
