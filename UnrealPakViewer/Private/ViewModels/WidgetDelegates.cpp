#include "WidgetDelegates.h"

FOnSwitchToTreeView& FWidgetDelegates::GetOnSwitchToTreeView()
{
	static FOnSwitchToTreeView Delegate;
	return Delegate;
}

FOnSwitchToFileView& FWidgetDelegates::GetOnSwitchToFileView()
{
	static FOnSwitchToFileView Delegate;
	return Delegate;
}
