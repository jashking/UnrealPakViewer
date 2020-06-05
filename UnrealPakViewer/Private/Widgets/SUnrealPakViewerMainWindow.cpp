#include "SUnrealPakViewerMainWindow.h"

#include "EditorStyleSet.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SUnrealPakViewerMainWindow"

static const FName TreeViewTabId("UnrealPakViewerTreeView");
static const FName FileViewTabId("UnrealPakViewerFileView");

void SUnrealPakViewerMainWindow::Construct(const FArguments& Args)
{
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab).TabRole(ETabRole::MajorTab);

	TabManager = FGlobalTabmanager::Get()->NewTabManager(DockTab);
	TSharedRef<FWorkspaceItem> AppMenuGroup = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("UnrealPakViewerMenuGroupName", "UnrealPak Viewer"));

	TabManager->RegisterTabSpawner(TreeViewTabId, FOnSpawnTab::CreateRaw(this, &SUnrealPakViewerMainWindow::OnSpawnTab, TreeViewTabId))
		.SetDisplayName(LOCTEXT("TreeViewTabTitle", "Tree View"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "UnrealPakViewer.Tabs.Tools"))
		.SetGroup(AppMenuGroup);

	TabManager->RegisterTabSpawner(FileViewTabId, FOnSpawnTab::CreateRaw(this, &SUnrealPakViewerMainWindow::OnSpawnTab, FileViewTabId))
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

	OnWindowClosed.BindRaw(this, &SUnrealPakViewerMainWindow::OnExit);
}

TSharedRef<SWidget> SUnrealPakViewerMainWindow::MakeMainMenu()
{
	// Create & initialize main menu.
	FMenuBarBuilder MenuBarBuilder = FMenuBarBuilder(nullptr);
	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("FileMenu", "File"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateRaw(this, &SUnrealPakViewerMainWindow::FillFileMenu)
	);

	MenuBarBuilder.AddPullDownMenu(
		LOCTEXT("OptionsMenu", "Options"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateRaw(this, &SUnrealPakViewerMainWindow::FillOptionsMenu)
	);

	// Create the menu bar
	TSharedRef<SWidget> MenuBarWidget = MenuBarBuilder.MakeWidget();

	return MenuBarWidget;
}

void SUnrealPakViewerMainWindow::FillFileMenu(class FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Open", "Open pak..."),
		LOCTEXT("Open_ToolTip", "Open an unreal pak file"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SUnrealPakViewerMainWindow::OnOpenPakFile),
			FCanExecuteAction()
		),
		NAME_None,
		EUserInterfaceActionType::Button
	);
}

void SUnrealPakViewerMainWindow::FillOptionsMenu(class FMenuBuilder& MenuBuilder)
{

}

TSharedRef<SDockTab> SUnrealPakViewerMainWindow::OnSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier)
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

void SUnrealPakViewerMainWindow::OnExit(const TSharedRef<SWindow>& InWindow)
{

}

void SUnrealPakViewerMainWindow::OnOpenPakFile()
{

}

#undef LOCTEXT_NAMESPACE
