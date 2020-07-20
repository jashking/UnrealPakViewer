#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "Widgets/SWindow.h"

class SExtractProgressWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SExtractProgressWindow)
	{
	}
	SLATE_ATTRIBUTE(FDateTime, StartTime)
	SLATE_END_ARGS()

	SExtractProgressWindow();
	virtual	~SExtractProgressWindow();

	/** Widget constructor */
	void Construct(const FArguments& Args);

	/**
	 * Ticks this widget. Override in derived classes, but always call the parent implementation.
	 *
	 * @param AllottedGeometry - The space allotted for this widget
	 * @param InCurrentTime - Current absolute real time
	 * @param InDeltaTime - Real time passed since last tick
	 */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:
	FORCEINLINE FText GetCompleteCount() const;
	FORCEINLINE FText GetErrorCount() const;
	FORCEINLINE FText GetTotalCount() const;
	FORCEINLINE TOptional<float> GetExtractProgress() const;
	FORCEINLINE FText GetExtractProgressText() const;
	FORCEINLINE FText GetTimeElapsed() const;

	void OnExit(const TSharedRef<SWindow>& InWindow);
	void OnUpdateExtractProgress(int32 InCompleteCount, int32 InErrorCount, int32 InTotalCount);

protected:
	int32 CompleteCount;
	int32 TotalCount;
	int32 ErrorCount;
	TAttribute<FDateTime> StartTime;
	FDateTime LastTime;
	bool bExtractFinished;
};