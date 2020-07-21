#include "SAboutWindow.h"

#include "EditorStyleSet.h"
#include "Launch/Resources/Version.h"

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
	const FVector2D InitialWindowDimensions(600, 60);

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

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SKeyValueRow).KeyStretchCoefficient(0.2f).KeyText(LOCTEXT("Built_With_Text", "Built With:")).ValueText(FText::Format(LOCTEXT("Engine_Version_Text", "UE {0}.{1}.{2}"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION))
				]

				//+ SVerticalBox::Slot()
				//.AutoHeight()
				//.HAlign(HAlign_Right)
				//.Padding(0.f, 4.f)
				//[
				//]
			]
		]
	);
}

#undef LOCTEXT_NAMESPACE
