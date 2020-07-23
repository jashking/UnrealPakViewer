#include "SMainWindow.h"

#include "Async/TaskGraphInterfaces.h"
#include "DesktopPlatformModule.h"
#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Launch/Resources/Version.h"
#include "Misc/DateTime.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Docking/SDockTab.h"

#include "CommonDefines.h"
#include "PakAnalyzerModule.h"
#include "SAboutWindow.h"
#include "SExtractProgressWindow.h"
#include "SKeyInputWindow.h"
#include "SOptionsWindow.h"
#include "SPakFileView.h"
#include "SPakSummaryView.h"
#include "SPakTreeView.h"
#include "UnrealPakViewerStyle.h"
#include "ViewModels/WidgetDelegates.h"

#define LOCTEXT_NAMESPACE "SMainWindow"

static const FName SummaryViewTabId("UnrealPakViewerSummaryView");
static const FName TreeViewTabId("UnrealPakViewerTreeView");
static const FName FileViewTabId("UnrealPakViewerFileView");

SMainWindow::SMainWindow()
{
	FWidgetDelegates::GetOnSwitchToFileViewDelegate().AddRaw(this, &SMainWindow::OnSwitchToFileView);
	FWidgetDelegates::GetOnSwitchToTreeViewDelegate().AddRaw(this, &SMainWindow::OnSwitchToTreeView);
	FPakAnalyzerDelegates::OnExtractStart.BindRaw(this, &SMainWindow::OnExtractStart);
}

SMainWindow::~SMainWindow()
{
	FWidgetDelegates::GetOnSwitchToFileViewDelegate().RemoveAll(this);
	FWidgetDelegates::GetOnSwitchToTreeViewDelegate().RemoveAll(this);
}

void SMainWindow::Construct(const FArguments& Args)
{
	GConfig->GetArray(TEXT("UnrealPakViewer"), TEXT("RecentFiles"), RecentFiles, GEngineIni);

	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab).TabRole(ETabRole::MajorTab);

	TabManager = FGlobalTabmanager::Get()->NewTabManager(DockTab);
	TSharedRef<FWorkspaceItem> AppMenuGroup = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("UnrealPakViewerMenuGroupName", "UnrealPak Viewer"));

	TabManager->RegisterTabSpawner(SummaryViewTabId, FOnSpawnTab::CreateRaw(this, &SMainWindow::OnSpawnTab_SummaryView))
		.SetDisplayName(LOCTEXT("SummaryViewTabTitle", "Summary View"))
		.SetIcon(FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Tab.Summary"))
		.SetGroup(AppMenuGroup);

	TabManager->RegisterTabSpawner(TreeViewTabId, FOnSpawnTab::CreateRaw(this, &SMainWindow::OnSpawnTab_TreeView))
		.SetDisplayName(LOCTEXT("TreeViewTabTitle", "Tree View"))
		.SetIcon(FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Tab.Tree"))
		.SetGroup(AppMenuGroup);

	TabManager->RegisterTabSpawner(FileViewTabId, FOnSpawnTab::CreateRaw(this, &SMainWindow::OnSpawnTab_FileView))
		.SetDisplayName(LOCTEXT("FileViewTabTitle", "File View"))
		.SetIcon(FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "Tab.File"))
		.SetGroup(AppMenuGroup);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("UnrealPakViewer_v1.0")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->AddTab(SummaryViewTabId, ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
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
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
				[
					TabManager->RestoreFrom(Layout, TSharedPtr<SWindow>()).ToSharedRef()
				]
			]
		]
	);

	OnWindowClosed.BindRaw(this, &SMainWindow::OnExit);

	FPakAnalyzerDelegates::OnGetAESKey.BindRaw(this, &SMainWindow::OnGetAESKey);
	FPakAnalyzerDelegates::OnLoadPakFailed.BindRaw(this, &SMainWindow::OnLoadPakFailed);
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
		LOCTEXT("ViewsMenu", "Views"),
		FText::GetEmpty(),
		FNewMenuDelegate::CreateRaw(this, &SMainWindow::FillViewsMenu)
	);

	MenuBarBuilder.AddMenuEntry(
		LOCTEXT("OptionsMenu", "Options"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SMainWindow::OnOpenOptionsDialog),
			FCanExecuteAction()
		),
		NAME_None,
		EUserInterfaceActionType::Button
	);

	MenuBarBuilder.AddMenuEntry(
		LOCTEXT("AboutMenu", "About"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SMainWindow::OnOpenAboutDialog),
			FCanExecuteAction()
		),
		NAME_None,
		EUserInterfaceActionType::Button
	);

	// Create the menu bar
	TSharedRef<SWidget> MenuBarWidget = MenuBarBuilder.MakeWidget();

	// Tell tab-manager about the multi-box for platforms with a global menu bar
	TabManager->SetMenuMultiBox(MenuBarBuilder.GetMultiBox());

	return MenuBarWidget;
}

void SMainWindow::FillFileMenu(class FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Load and Save", LOCTEXT("LoadText", "Load"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Open", "Open pak..."),
			LOCTEXT("Open_ToolTip", "Open an unreal pak file."),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "LoadPak"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SMainWindow::OnLoadPakFile),
				FCanExecuteAction()
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Recent files", LOCTEXT("RecentFilesText", "Recent files"));
	for (const FString& File : RecentFiles)
	{
		MenuBuilder.AddMenuEntry(
			FText::FromString(FPaths::GetCleanFilename(File)),
			FText::FromString(File),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "LoadPak"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SMainWindow::OnLoadRecentFile, File),
				FCanExecuteAction::CreateSP(this, &SMainWindow::OnLoadRecentFileCanExecute, File)
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();
}

void SMainWindow::FillViewsMenu(class FMenuBuilder& MenuBuilder)
{
	if (!TabManager.IsValid())
	{
		return;
	}

#if !WITH_EDITOR
	//FGlobalTabmanager::Get()->PopulateTabSpawnerMenu(MenuBuilder, WorkspaceMenu::GetMenuStructure().GetStructureRoot());
#endif //!WITH_EDITOR

	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
}

TSharedRef<class SDockTab> SMainWindow::OnSpawnTab_SummaryView(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(true)
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SPakSummaryView)
		];

	return DockTab;
}

TSharedRef<class SDockTab> SMainWindow::OnSpawnTab_TreeView(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SPakTreeView)
		];

	return DockTab;
}

TSharedRef<class SDockTab> SMainWindow::OnSpawnTab_FileView(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SPakFileView)
		];

	return DockTab;
}

void SMainWindow::OnExit(const TSharedRef<SWindow>& InWindow)
{
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
		LoadPakFile(OutFiles[0]);
	}
}

void SMainWindow::OnLoadPakFailed(const FString& InReason)
{
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(InReason));
}

FString SMainWindow::OnGetAESKey()
{
	FString EncryptionKey = TEXT("");

	TSharedPtr<SKeyInputWindow> KeyInputWindow =
		SNew(SKeyInputWindow)
		.OnConfirm_Lambda([&EncryptionKey](const FString& InEncryptionKey) { EncryptionKey = InEncryptionKey; });

	FSlateApplication::Get().AddModalWindow(KeyInputWindow.ToSharedRef(), SharedThis(this), false);

	return EncryptionKey;
}

void SMainWindow::OnSwitchToTreeView(const FString& InPath)
{
#if ENGINE_MINOR_VERSION >= 26
	TSharedPtr<SDockTab> TreeViewTab = TabManager->TryInvokeTab(TreeViewTabId);
#else
	TSharedPtr<SDockTab> TreeViewTab = TabManager->InvokeTab(TreeViewTabId);
#endif
	if (TreeViewTab.IsValid())
	{
		TSharedRef<SPakTreeView> TreeView = StaticCastSharedRef<SPakTreeView>(TreeViewTab->GetContent());
		TreeView->SetDelayHighlightItem(InPath);
	}
}

void SMainWindow::OnSwitchToFileView(const FString& InPath)
{
#if ENGINE_MINOR_VERSION >= 26
	TSharedPtr<SDockTab> FileViewTab = TabManager->TryInvokeTab(FileViewTabId);
#else
	TSharedPtr<SDockTab> FileViewTab = TabManager->InvokeTab(FileViewTabId);
#endif
	if (FileViewTab.IsValid())
	{
		TSharedRef<SPakFileView> FileView = StaticCastSharedRef<SPakFileView>(FileViewTab->GetContent());
		FileView->SetDelayHighlightItem(InPath);
	}
}

void SMainWindow::OnExtractStart()
{
	FFunctionGraphTask::CreateAndDispatchWhenReady([this]()
		{
			TSharedPtr<SExtractProgressWindow> ExtractProgressWindow = SNew(SExtractProgressWindow).StartTime(FDateTime::Now());

			FSlateApplication::Get().AddModalWindow(ExtractProgressWindow.ToSharedRef(), SharedThis(this), true);
			ExtractProgressWindow->ShowWindow();
		},
		TStatId(), nullptr, ENamedThreads::GameThread);
}

void SMainWindow::OnLoadRecentFile(FString InPath)
{
	LoadPakFile(InPath);
}

bool SMainWindow::OnLoadRecentFileCanExecute(FString InPath) const
{
	return FPaths::FileExists(InPath);
}

void SMainWindow::OnOpenOptionsDialog()
{
	TSharedPtr<SOptionsWindow> OptionsWindow = SNew(SOptionsWindow);

	FSlateApplication::Get().AddModalWindow(OptionsWindow.ToSharedRef(), SharedThis(this), true);
	OptionsWindow->ShowWindow();
}

void SMainWindow::OnOpenAboutDialog()
{
	TSharedPtr<SAboutWindow> AboutWindow = SNew(SAboutWindow);

	FSlateApplication::Get().AddModalWindow(AboutWindow.ToSharedRef(), SharedThis(this), true);
	AboutWindow->ShowWindow();
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
					LoadPakFile(Files[0]);
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

void SMainWindow::LoadPakFile(const FString& PakFilePath)
{
	static const int32 MAX_RECENT_FILE_COUNT = 5;

	const FString FullPath = FPaths::ConvertRelativePathToFull(PakFilePath);

	const bool bLoadResult = IPakAnalyzerModule::Get().GetPakAnalyzer()->LoadPakFile(FullPath);
	if (bLoadResult)
	{
		RecentFiles.Remove(FullPath);
		RecentFiles.Insert(FullPath, 0);
		if (RecentFiles.Num() > MAX_RECENT_FILE_COUNT)
		{
			RecentFiles.SetNum(MAX_RECENT_FILE_COUNT); // keep max 5 recent files
		}
		
		GConfig->SetArray(TEXT("UnrealPakViewer"), TEXT("RecentFiles"), RecentFiles, GEngineIni);
		GConfig->Flush(false, GEngineIni);
	}
}

#undef LOCTEXT_NAMESPACE
