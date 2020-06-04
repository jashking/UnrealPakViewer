#include "SUnrealPakViewerMainWindow.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SUnrealPakViewerMainWindow"

void SUnrealPakViewerMainWindow::Construct(const FArguments& Args)
{
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);

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
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			]
		]
	);
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

void SUnrealPakViewerMainWindow::OnOpenPakFile()
{

}

#undef LOCTEXT_NAMESPACE
