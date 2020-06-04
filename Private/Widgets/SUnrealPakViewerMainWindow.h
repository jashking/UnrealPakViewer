#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"

class SUnrealPakViewerMainWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SUnrealPakViewerMainWindow)
	{
	}
	SLATE_END_ARGS()

	SUnrealPakViewerMainWindow() {}

	/** Widget constructor */
	void Construct(const FArguments& Args);

protected:
	FReply Close();

	TSharedRef<SWidget> MakeMainMenu();
	void FillFileMenu(class FMenuBuilder& MenuBuilder);
	void FillOptionsMenu(class FMenuBuilder& MenuBuilder);

	void OnOpenPakFile();

protected:
	static const int32 WINDOW_WIDTH = 1200;
	static const int32 WINDOW_HEIGHT = 800;
};
