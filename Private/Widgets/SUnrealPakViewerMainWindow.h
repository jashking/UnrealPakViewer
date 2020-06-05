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
	TSharedRef<SWidget> MakeMainMenu();
	void FillFileMenu(class FMenuBuilder& MenuBuilder);
	void FillOptionsMenu(class FMenuBuilder& MenuBuilder);
	TSharedRef<class SDockTab> OnSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier);

	void OnExit(const TSharedRef<SWindow>& InWindow);
	void OnOpenPakFile();

protected:
	static const int32 WINDOW_WIDTH = 1200;
	static const int32 WINDOW_HEIGHT = 800;

	/** Holds the tab manager that manages the front-end's tabs. */
	TSharedPtr<class FTabManager> TabManager;
};
