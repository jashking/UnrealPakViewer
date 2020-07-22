#include "WidgetDelegates.h"

FOnSwitchToTreeView& FWidgetDelegates::GetOnSwitchToTreeViewDelegate()
{
	static FOnSwitchToTreeView Delegate;
	return Delegate;
}

FOnSwitchToFileView& FWidgetDelegates::GetOnSwitchToFileViewDelegate()
{
	static FOnSwitchToFileView Delegate;
	return Delegate;
}

FOnLoadAssetRegistryFinished& FWidgetDelegates::GetOnLoadAssetRegistryFinishedDelegate()
{
	static FOnLoadAssetRegistryFinished Delegate;
	return Delegate;
}