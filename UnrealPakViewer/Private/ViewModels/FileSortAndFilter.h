#pragma once

#include "Async/AsyncWork.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"

#include "PakFileEntry.h"

class SPakFileView;

class FFileSortAndFilterTask : public FNonAbandonableTask
{
public:
	FFileSortAndFilterTask(FName InSortedColumn, EColumnSortMode::Type InSortMode, TSharedPtr<SPakFileView> InPakFileView)
		: CurrentSortedColumn(InSortedColumn)
		, CurrentSortMode(InSortMode)
		, WeakPakFileView(InPakFileView)
	{

	}

	void DoWork();
	void SetSortInfo(FName InSortedColumn, EColumnSortMode::Type InSortMode);

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(STAT_FFileSortAndFilterTask, STATGROUP_ThreadPoolAsyncTasks);
	}

protected:
	FName CurrentSortedColumn;
	EColumnSortMode::Type CurrentSortMode;

	/** Shared pointer to parent PakFileView widget. Used for accesing the cache and to check if cancel is requested. */
	TWeakPtr<SPakFileView> WeakPakFileView;
};