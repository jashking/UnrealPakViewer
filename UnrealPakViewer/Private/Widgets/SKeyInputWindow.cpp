#include "SKeyInputWindow.h"

#include "EditorStyle.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "SKeyInputWindow"

SKeyInputWindow::SKeyInputWindow()
{

}

SKeyInputWindow::~SKeyInputWindow()
{

}

void SKeyInputWindow::Construct(const FArguments& Args)
{
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
	const FVector2D InitialWindowDimensions(600, 50);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Encryption Key"))
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
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(EHorizontalAlignment::HAlign_Left)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
				[
					SNew(STextBlock).Text(LOCTEXT("KeyText", "Please input encryption key(Base64):"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
				[
					SAssignNew(EncryptionKeyBox, SEditableTextBox)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(EHorizontalAlignment::HAlign_Right)
				.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
				[
					SNew(SBox)
					.MinDesiredWidth(60.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("ConfirmButtonText", "OK"))
						.HAlign(EHorizontalAlignment::HAlign_Center)
						.VAlign(EVerticalAlignment::VAlign_Center)
						.OnClicked(this, &SKeyInputWindow::OnConfirmButtonClicked)
					]
				]
			]
		]
	);

	OnConfirm = Args._OnConfirm;
}

FReply SKeyInputWindow::OnConfirmButtonClicked()
{
	OnConfirm.ExecuteIfBound(EncryptionKeyBox->GetText().ToString());

	RequestDestroyWindow();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
