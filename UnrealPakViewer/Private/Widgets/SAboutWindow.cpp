#include "SAboutWindow.h"

#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Launch/Resources/Version.h"
#include "Widgets/Input/SHyperlink.h"

#include "SKeyValueRow.h"

#define LOCTEXT_NAMESPACE "SAboutWindow"

SAboutWindow::SAboutWindow()
{
}

SAboutWindow::~SAboutWindow()
{
}

void SAboutWindow::Construct(const FArguments& Args)
{
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
	const FVector2D InitialWindowDimensions(200, 50);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "About"))
		.HasCloseButton(true)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize)
		.ClientSize(InitialWindowDimensions * DPIScaleFactor)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
			.Padding(FMargin(5.f, 10.f))
			[
				SNew(SVerticalBox)

#if defined(UNREAL_PAK_VIEWER_VERSION)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SKeyValueRow).KeyText(LOCTEXT("Version_Text", "Version:")).ValueText(FText::FromString(UNREAL_PAK_VIEWER_VERSION))
				]
#endif

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SKeyValueRow).KeyText(LOCTEXT("Engine_Text", "Engine:")).ValueText(FText::Format(LOCTEXT("Engine_Version_Text", "{0}.{1}.{2}"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION))
				]
			]
		]
	);
}

#undef LOCTEXT_NAMESPACE
