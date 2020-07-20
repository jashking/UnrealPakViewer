#include "SExtractProgressWindow.h"

#include "EditorStyle.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/Timespan.h"

#include "CommonDefines.h"
#include "PakAnalyzerModule.h"
#include "SKeyValueRow.h"

#define LOCTEXT_NAMESPACE "SExtractProgressWindow"

SExtractProgressWindow::SExtractProgressWindow()
	: CompleteCount(0)
	, TotalCount(0)
	, ErrorCount(0)
	, bExtractFinished(false)
{
	FPakAnalyzerDelegates::OnUpdateExtractProgress.BindRaw(this, &SExtractProgressWindow::OnUpdateExtractProgress);
}

SExtractProgressWindow::~SExtractProgressWindow()
{

}

void SExtractProgressWindow::Construct(const FArguments& Args)
{
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
	const FVector2D InitialWindowDimensions(600, 50);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Extracting..."))
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
						SNew(STextBlock).Text(LOCTEXT("ExtractText", "Extract progress:"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.Padding(FMargin(0.f, 0.f, 5.f, 0.f))
					[
						SNew(SOverlay)

						+ SOverlay::Slot()
						[
							SNew(SProgressBar).Percent(this, &SExtractProgressWindow::GetExtractProgress)
						]

						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &SExtractProgressWindow::GetExtractProgressText)
							.ColorAndOpacity(FLinearColor::Black)
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 4.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyStretchCoefficient(1.f).KeyText(LOCTEXT("CompleteText", "Complete:")).ValueText(this, &SExtractProgressWindow::GetCompleteCount)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyStretchCoefficient(1.f).KeyText(LOCTEXT("ErrorText", "Error:")).ValueText(this, &SExtractProgressWindow::GetErrorCount)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyStretchCoefficient(1.f).KeyText(LOCTEXT("Total", "Total:")).ValueText(this, &SExtractProgressWindow::GetTotalCount)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyStretchCoefficient(0.8f).KeyText(LOCTEXT("Time", "Time:")).ValueText(this, &SExtractProgressWindow::GetTimeElapsed)
					]
				]
			]
		]
	);

	OnWindowClosed.BindRaw(this, &SExtractProgressWindow::OnExit);
	StartTime = Args._StartTime;
	LastTime = Args._StartTime.Get();
	bExtractFinished = false;

	SetCanTick(true);
}

void SExtractProgressWindow::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SWindow::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	bExtractFinished = CompleteCount == TotalCount;
	if (!bExtractFinished)
	{
		LastTime = FDateTime::Now();
	}
}

FORCEINLINE FText SExtractProgressWindow::GetCompleteCount() const
{
	return FText::AsNumber(CompleteCount);
}

FORCEINLINE FText SExtractProgressWindow::GetErrorCount() const
{
	return FText::AsNumber(ErrorCount);
}

FORCEINLINE FText SExtractProgressWindow::GetTotalCount() const
{
	return FText::AsNumber(TotalCount);
}

FORCEINLINE TOptional<float> SExtractProgressWindow::GetExtractProgress() const
{
	return TotalCount > 0 ? (float)CompleteCount / TotalCount : 0.f;
}

FORCEINLINE FText SExtractProgressWindow::GetExtractProgressText() const
{
	return TotalCount > 0 ? FText::FromString(FString::Printf(TEXT("%.2f%%"), (float)CompleteCount / TotalCount * 100)) : FText();
}

FORCEINLINE FText SExtractProgressWindow::GetTimeElapsed() const
{
	const FTimespan ElapsedTime = LastTime - StartTime.Get();

	return FText::FromString(ElapsedTime.ToString());
}

void SExtractProgressWindow::OnExit(const TSharedRef<SWindow>& InWindow)
{
	IPakAnalyzerModule::Get().GetPakAnalyzer()->CancelExtract();
}

void SExtractProgressWindow::OnUpdateExtractProgress(int32 InCompleteCount, int32 InErrorCount, int32 InTotalCount)
{
	CompleteCount = InCompleteCount;
	ErrorCount = InErrorCount;
	TotalCount = InTotalCount;
}

#undef LOCTEXT_NAMESPACE
