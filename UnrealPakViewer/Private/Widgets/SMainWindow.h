#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"

class SMainWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SMainWindow)
	{
	}
	SLATE_END_ARGS()

	SMainWindow();
	virtual	~SMainWindow();

	/** Widget constructor */
	void Construct(const FArguments& Args);

protected:
	TSharedRef<SWidget> MakeMainMenu();
	void FillFileMenu(class FMenuBuilder& MenuBuilder);
	void FillViewsMenu(class FMenuBuilder& MenuBuilder);

	TSharedRef<class SDockTab> OnSpawnTab_SummaryView(const FSpawnTabArgs& Args);
	TSharedRef<class SDockTab> OnSpawnTab_TreeView(const FSpawnTabArgs& Args);
	TSharedRef<class SDockTab> OnSpawnTab_FileView(const FSpawnTabArgs& Args);

	void OnExit(const TSharedRef<SWindow>& InWindow);
	void OnLoadPakFile();
	void OnLoadPakFailed(const FString& InReason);
	FString OnGetAESKey();
	void OnSwitchToTreeView(const FString& InPath);
	void OnSwitchToFileView(const FString& InPath);
	void OnExtractStart();
	void OnLoadRecentFile(FString InPath);
	bool OnLoadRecentFileCanExecute(FString InPath) const;

	void OnOpenOptionsDialog();
	void OnOpenAboutDialog();

	/**
	 * Called when the user is dropping something onto a widget; terminates drag and drop.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	/**
	 * Called during drag and drop when the the mouse is being dragged over a widget.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)  override;

	void LoadPakFile(const FString& PakFilePath);

protected:
	static const int32 WINDOW_WIDTH = 1200;
	static const int32 WINDOW_HEIGHT = 800;

	/** Holds the tab manager that manages the front-end's tabs. */
	TSharedPtr<class FTabManager> TabManager;

	TArray<FString> RecentFiles;
};
