#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSwitchToTreeView, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSwitchToFileView, const FString&);
DECLARE_MULTICAST_DELEGATE(FOnLoadAssetRegistryFinished);

class FWidgetDelegates
{
public:
	static FOnSwitchToTreeView& GetOnSwitchToTreeViewDelegate();
	static FOnSwitchToFileView& GetOnSwitchToFileViewDelegate();
	static FOnLoadAssetRegistryFinished& GetOnLoadAssetRegistryFinishedDelegate();
};