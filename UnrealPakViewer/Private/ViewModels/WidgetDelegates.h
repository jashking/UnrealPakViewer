#pragma once

#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSwitchToTreeView, const FString&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSwitchToFileView, const FString&);

class FWidgetDelegates
{
public:
	static FOnSwitchToTreeView& GetOnSwitchToTreeView();
	static FOnSwitchToFileView& GetOnSwitchToFileView();
};