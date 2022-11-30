#include "SKeyInputWindow.h"

//#include "EditorStyle.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/Base64.h"
#include "Misc/Char.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "SKeyInputWindow"

SKeyInputWindow::SKeyInputWindow()
{

}

SKeyInputWindow::~SKeyInputWindow()
{

}

void SKeyInputWindow::Construct(const FArguments& Args)
{
	OnConfirm = Args._OnConfirm;
	PakPath = Args._PakPath;
	PakGuid = Args._PakGuid;

	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
	const FVector2D InitialWindowDimensions(600, 70);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Please input encryption key(Base64 string or Hex string:"))
		.HasCloseButton(true)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize)
		.ClientSize(InitialWindowDimensions * DPIScaleFactor)
		[
			SNew(SBorder)
			//.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
			.Padding(FMargin(5.f, 10.f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				. AutoHeight()
				.HAlign(EHorizontalAlignment::HAlign_Left)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Padding(FMargin(0.f, 0.f, 0.f, 5.f))
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s(Guid: %s):"), *FPaths::GetCleanFilename(PakPath.Get(TEXT(""))), *LexToString(PakGuid.Get(FGuid())))))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)

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
		]
	);
}

FReply SKeyInputWindow::OnConfirmButtonClicked()
{
	FString InputString = EncryptionKeyBox->GetText().ToString();
	if (InputString.StartsWith(TEXT("0x")))
	{
		InputString.RemoveAt(0, 2);
	}

	bool bIsPureHexDigit = true;
	const TArray<TCHAR>& CharArray = InputString.GetCharArray();
	for (int32 i = 0; i < CharArray.Num() - 1; ++i)
	{
		if (!FChar::IsHexDigit(CharArray[i]))
		{
			bIsPureHexDigit = false;
			break;
		}
	}

	uint8 Hexs[32] = { 0 };
	if (bIsPureHexDigit && FString::ToHexBlob(InputString, Hexs, sizeof(Hexs)))
	{
		InputString = FBase64::Encode(Hexs, sizeof(Hexs));
	}

	OnConfirm.ExecuteIfBound(InputString);

	RequestDestroyWindow();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
