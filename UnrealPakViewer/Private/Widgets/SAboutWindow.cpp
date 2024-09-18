#include "SAboutWindow.h"

//#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Widgets/Input/SHyperlink.h"

#include "SKeyValueRow.h"

#define LOCTEXT_NAMESPACE "SAboutWindow"

#define GITHUB_HOME TEXT("https://github.com/jashking/UnrealPakViewer")

SAboutWindow::SAboutWindow()
{
}

SAboutWindow::~SAboutWindow()
{
}

void SAboutWindow::Construct(const FArguments& Args)
{
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
	const FVector2D InitialWindowDimensions(360, 80);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "About"))
		.HasCloseButton(true)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize)
		.ClientSize(InitialWindowDimensions * DPIScaleFactor)
		[
			SNew(SBorder)
			//.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
			.Padding(FMargin(5.f, 5.f))
			[
				SNew(SVerticalBox)

#if defined(UNREAL_PAK_VIEWER_VERSION)
				+SVerticalBox::Slot()
				.Padding(0.f, 2.f)
				.AutoHeight()
				[
					SNew(SKeyValueRow).KeyText(LOCTEXT("Version_Text", "Version:")).ValueText(FText::FromString(UNREAL_PAK_VIEWER_VERSION))
				]
#endif

				+ SVerticalBox::Slot()
				.Padding(0.f, 2.f)
				.AutoHeight()
				[
					SNew(SKeyValueRow).KeyText(LOCTEXT("Engine_Text", "Engine:")).ValueText(FText::Format(LOCTEXT("Engine_Version_Text", "{0}.{1}.{2}"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION))
				]

				+ SVerticalBox::Slot()
				.Padding(0.f, 2.f)
				.AutoHeight()
				[
					SNew(SKeyValueRow).KeyText(LOCTEXT("Github_Text", "Github:"))
					.ValueContent()
					[
						SNew(SHyperlink).Text(FText::FromString(GITHUB_HOME))
						.OnNavigate_Lambda([]()
						{
							FPlatformProcess::LaunchURL(GITHUB_HOME, NULL, NULL);
						})
					]
				]
			]
		]
	);
}

#undef LOCTEXT_NAMESPACE
