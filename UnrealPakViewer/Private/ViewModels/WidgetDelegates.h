#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSwitchToTreeView, const FString&, int32);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSwitchToFileView, const FString&, int32);
DECLARE_MULTICAST_DELEGATE(FOnLoadAssetRegistryFinished);

class FWidgetDelegates
{
public:
	static FOnSwitchToTreeView& GetOnSwitchToTreeViewDelegate();
	static FOnSwitchToFileView& GetOnSwitchToFileViewDelegate();
	static FOnLoadAssetRegistryFinished& GetOnLoadAssetRegistryFinishedDelegate();
};