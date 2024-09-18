#include "SMainWindow.h"

#include "Async/TaskGraphInterfaces.h"
#include "DesktopPlatformModule.h"
//#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformFile.h"
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
	LoadConfig();

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
				//.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
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
	TabManager->SetMenuMultiBox(MenuBarBuilder.GetMultiBox(), MenuBarWidget);

	return MenuBarWidget;
}

void SMainWindow::FillFileMenu(class FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Load and Save", LOCTEXT("LoadText", "Load"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("LoadPak", "Load pak/ucas files..."),
			LOCTEXT("LoadPak_ToolTip", "Load unreal pak/ucas files."),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "LoadPak"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SMainWindow::OnLoadPakFile),
				FCanExecuteAction()
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("LoadDirectory", "Load directory..."),
			LOCTEXT("LoadDirectory_ToolTip", "Load all pak/ucas files in directory."),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "FolderClosed"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SMainWindow::OnLoadAllFilesInFolder),
				FCanExecuteAction()
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("LoadCookedFolder", "Load cooked folder..."),
			LOCTEXT("LoadCookedFolder_ToolTip", "Load an cooked output folder."),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "FolderClosed"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SMainWindow::OnLoadFolder),
				FCanExecuteAction()
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Recent files", LOCTEXT("RecentFilesText", "Recent files"));
	for (int32 i = 0; i < RecentFiles.Num(); ++i)
	{
		const FString& RecentInfo = RecentFiles[i];

		MenuBuilder.AddMenuEntry(
			FText::FromString(FPaths::GetCleanFilename(RecentInfo)),
			FText::FromString(RecentInfo),
			FSlateIcon(FUnrealPakViewerStyle::GetStyleSetName(), "LoadPak"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SMainWindow::OnLoadRecentFile, i),
				FCanExecuteAction::CreateSP(this, &SMainWindow::OnLoadRecentFileCanExecute, i)
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
#if ENABLE_IO_STORE_ANALYZER
			LOCTEXT("LoadPak_FileFilter", "Pak files (*.pak, *.ucas)|*.pak;*.ucas|All files (*.*)|*.*").ToString(),
#else
			LOCTEXT("LoadPak_FileFilter", "Pak files (*.pak)|*.pak|All files (*.*)|*.*").ToString(),
#endif
			EFileDialogFlags::Multiple,
			OutFiles
		);
	}

	if (bOpened && OutFiles.Num() > 0)
	{
		LoadPakFile(OutFiles);
	}
}

void SMainWindow::OnLoadAllFilesInFolder()
{
	FString OutFolder;
	bool bOpened = false;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FSlateApplication::Get().CloseToolTip();

		bOpened = DesktopPlatform->OpenDirectoryDialog
		(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("LoadAlllFiles_FolderDesc", "Load all pak/ucas files...").ToString(),
			TEXT(""),
			OutFolder
		);
	}

	if (bOpened)
	{
		TArray<FString> ContainerFilePaths;
		
		TArray<FString> FoundContainerFiles;
		IFileManager::Get().FindFilesRecursive(FoundContainerFiles, *OutFolder, TEXT("*.ucas"), true, false);
		for (const FString& Filename : FoundContainerFiles)
		{
			ContainerFilePaths.Emplace(Filename);
		}

		TArray<FString> FoundPakFiles;
		IFileManager::Get().FindFilesRecursive(FoundPakFiles, *OutFolder, TEXT("*.pak"), true, false);
		for (const FString& Filename : FoundPakFiles)
		{
			ContainerFilePaths.Emplace(Filename);
		}
		
		LoadPakFile(ContainerFilePaths);
	}
}

void SMainWindow::OnLoadFolder()
{
	FString OutFolder;
	bool bOpened = false;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FSlateApplication::Get().CloseToolTip();

		bOpened = DesktopPlatform->OpenDirectoryDialog
		(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("LoadPak_FolderDesc", "Open cooked folder...").ToString(),
			TEXT(""),
			OutFolder
		);
	}

	if (bOpened)
	{
		TArray<FString> Folders = { OutFolder };
		LoadPakFile(Folders);
	}
}

void SMainWindow::OnLoadPakFailed(const FString& InReason)
{
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(InReason));
}

FString SMainWindow::OnGetAESKey(const FString& InPakPath, const FGuid& PakGuid, bool& bCancel)
{
	FString EncryptionKey = TEXT("");
	bCancel = true;

	TSharedPtr<SKeyInputWindow> KeyInputWindow =
		SNew(SKeyInputWindow)
		.PakPath(InPakPath)
		.PakGuid(PakGuid)
		.OnConfirm_Lambda([&EncryptionKey, &bCancel](const FString& InEncryptionKey) { EncryptionKey = InEncryptionKey; bCancel = false; });

	FSlateApplication::Get().AddModalWindow(KeyInputWindow.ToSharedRef(), SharedThis(this), false);

	return EncryptionKey;
}

void SMainWindow::OnSwitchToTreeView(const FString& InPath, int32 PakIndex)
{
	TSharedPtr<SDockTab> TreeViewTab = TabManager->TryInvokeTab(TreeViewTabId);
	if (TreeViewTab.IsValid())
	{
		TSharedRef<SPakTreeView> TreeView = StaticCastSharedRef<SPakTreeView>(TreeViewTab->GetContent());
		TreeView->SetDelayHighlightItem(InPath, PakIndex);
	}
}

void SMainWindow::OnSwitchToFileView(const FString& InPath, int32 PakIndex)
{
	TSharedPtr<SDockTab> FileViewTab = TabManager->TryInvokeTab(FileViewTabId);
	if (FileViewTab.IsValid())
	{
		TSharedRef<SPakFileView> FileView = StaticCastSharedRef<SPakFileView>(FileViewTab->GetContent());
		FileView->SetDelayHighlightItem(InPath, PakIndex);
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

void SMainWindow::OnLoadRecentFile(int32 InIndex)
{
	if (RecentFiles.IsValidIndex(InIndex))
	{
		TArray<FString> PakFiles = { RecentFiles[InIndex] };
		LoadPakFile(PakFiles);
	}
}

bool SMainWindow::OnLoadRecentFileCanExecute(int32 InIndex) const
{
	if (!RecentFiles.IsValidIndex(InIndex))
	{
		return false;
	}

	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	return PlatformFile.FileExists(*RecentFiles[InIndex]) || PlatformFile.DirectoryExists(*RecentFiles[InIndex]);
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

			TArray<FString> PakFiles;
			for (const FString File : Files)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(File, true).ToLower();
#if ENABLE_IO_STORE_ANALYZER
				if (DraggedFileExtension == TEXT(".pak") || DraggedFileExtension == TEXT(".ucas"))
#else
				if (DraggedFileExtension == TEXT(".pak"))
#endif
				{
					PakFiles.Add(File);
				}
			}

			if (PakFiles.Num() > 0)
			{
				LoadPakFile(PakFiles);
				return FReply::Handled();
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
			for (const FString File : Files)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(File, true).ToLower();
#if ENABLE_IO_STORE_ANALYZER
				if (DraggedFileExtension == TEXT(".pak") || DraggedFileExtension == TEXT(".ucas"))
#else
				if (DraggedFileExtension == TEXT(".pak"))
#endif
				{
					return FReply::Handled();
				}
			}

			return FReply::Unhandled();
		}
	}

	return SCompoundWidget::OnDragOver(MyGeometry, DragDropEvent);
}

void SMainWindow::LoadPakFile(const TArray<FString>& PakFilePaths)
{
	static const int32 MAX_RECENT_FILE_COUNT = 30;

	TArray<FString> PakFiles;
	TArray<FString> CachedAESKeys;
	for (const FString& PakFilePath : PakFilePaths)
	{
		const FString FullPath = FPaths::ConvertRelativePathToFull(PakFilePath);
		PakFiles.Add(FullPath);
		CachedAESKeys.Add(FindExistingAESKey(FullPath));
	}

	if (PakFiles.Num() <= 0)
	{
		return;
	}

	IPakAnalyzerModule::Get().InitializeAnalyzerBackend(PakFiles[0]);

	const bool bLoadResult = IPakAnalyzerModule::Get().GetPakAnalyzer()->LoadPakFiles(PakFiles, CachedAESKeys);
	if (bLoadResult)
	{
		const TArray<FPakFileSumaryPtr>& Summaries = IPakAnalyzerModule::Get().GetPakAnalyzer()->GetPakFileSumary();
		for (const FPakFileSumaryPtr& Summary : Summaries)
		{
			if (!Summary.IsValid())
			{
				continue;
			}

			RemoveRecentFile(Summary->PakFilePath);
			if (!Summary->DecryptAESKeyStr.IsEmpty())
			{
				AESKeyCaches.Add(Summary->PakFilePath, Summary->DecryptAESKeyStr);
			}

			RecentFiles.Insert(Summary->PakFilePath, 0);
			if (RecentFiles.Num() > MAX_RECENT_FILE_COUNT)
			{
				RecentFiles.SetNum(MAX_RECENT_FILE_COUNT);
			}

			SaveConfig();
		}
	}
}

void SMainWindow::RemoveRecentFile(const FString& InFullPath)
{
	RecentFiles.RemoveAll([&InFullPath](const FString& RecentFile){
		return RecentFile.Equals(InFullPath, ESearchCase::IgnoreCase);
	});
}

void SMainWindow::SaveConfig()
{
	GConfig->SetArray(TEXT("UnrealPakViewer"), TEXT("RecentFiles"), RecentFiles, GGameIni);

	TArray<FString> KeyCaches;
	for (const TPair<FString, FString> KeyCache : AESKeyCaches)
	{
		KeyCaches.Add(FString::Printf(TEXT("%s %s"), *KeyCache.Value, *KeyCache.Key));
	}

	GConfig->SetArray(TEXT("UnrealPakViewer"), TEXT("KeyCaches"), KeyCaches, GGameIni);

	GConfig->Flush(false, GGameIni);
}

void SMainWindow::LoadConfig()
{
	RecentFiles.Empty();
	GConfig->GetArray(TEXT("UnrealPakViewer"), TEXT("RecentFiles"), RecentFiles, GGameIni);

	AESKeyCaches.Empty();

	TArray<FString> KeyCaches;
	GConfig->GetArray(TEXT("UnrealPakViewer"), TEXT("KeyCaches"), KeyCaches, GGameIni);
	for (const FString KeyCache : KeyCaches)
	{
		TArray<FString> Components;
		KeyCache.ParseIntoArray(Components, TEXT(" "));
		if (Components.Num() >= 2)
		{
			AESKeyCaches.Add(Components[1], Components[0]);
		}
	}
}

FString SMainWindow::FindExistingAESKey(const FString& InFullPath)
{
	FString* ExistingKey = AESKeyCaches.Find(InFullPath);
	if (ExistingKey)
	{
		return *ExistingKey;
	}

	return TEXT("");
}

#undef LOCTEXT_NAMESPACE
