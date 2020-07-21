#include "SOptionsWindow.h"

#include "EditorStyleSet.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformApplicationMisc.h"

#include "CommonDefines.h"
#include "PakAnalyzerModule.h"

#define LOCTEXT_NAMESPACE "SOptionsWindow"

SOptionsWindow::SOptionsWindow()
{
}

SOptionsWindow::~SOptionsWindow()
{
}

void SOptionsWindow::Construct(const FArguments& Args)
{
	int32 DefaultThreadCount = DEFAULT_EXTRACT_THREAD_COUNT;
	GConfig->GetInt(TEXT("UnrealPakViewer"), TEXT("ExtractThreadCount"), DefaultThreadCount, GEngineIni);

	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
	const FVector2D InitialWindowDimensions(600, 60);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Options"))
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
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(EHorizontalAlignment::HAlign_Left)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
					[
						SNew(STextBlock).Text(LOCTEXT("ExtractThreadCountText", "Extract thread count:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
					[
						SAssignNew(ThreadCountBox, SSpinBox<int32>).MinValue(1).MaxValue(FPlatformMisc::NumberOfCoresIncludingHyperthreads()).Value(DefaultThreadCount)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
					[
						SNew(STextBlock).Text(FText::Format(LOCTEXT("LimitRangeText", "(1 ~ {0})"), FPlatformMisc::NumberOfCoresIncludingHyperthreads()))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(0.f, 4.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
					[
						SNew(SBox).MinDesiredWidth(50)
						[
							SNew(SButton).HAlign(HAlign_Center).Text(LOCTEXT("ApplyText", "Apply")).OnClicked(this, &SOptionsWindow::OnApply)
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
					[
						SNew(SBox).MinDesiredWidth(50)
						[
							SNew(SButton).HAlign(HAlign_Center).Text(LOCTEXT("CancelText", "Cancel")).OnClicked(this, &SOptionsWindow::OnCancel)
						]
					]
				]
			]
		]
	);
}

FReply SOptionsWindow::OnApply()
{
	const int32 ThreadCount = ThreadCountBox->GetValueAttribute().Get();
	GConfig->SetInt(TEXT("UnrealPakViewer"), TEXT("ExtractThreadCount"), ThreadCount, GEngineIni);
	GConfig->Flush(false, GEngineIni);

	IPakAnalyzerModule::Get().GetPakAnalyzer()->SetExtractThreadCount(ThreadCount);

	RequestDestroyWindow();

	return FReply::Handled();
}

FReply SOptionsWindow::OnCancel()
{
	RequestDestroyWindow();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
