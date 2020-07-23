#pragma once

#include "Async/AsyncWork.h"
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Stats/Stats.h"

#include "PakFileEntry.h"

DECLARE_DELEGATE_FourParams(FOnSortAndFilterFinished, const FName, EColumnSortMode::Type, const FString&, const FString&);

class SPakFileView;

class FFileSortAndFilterTask : public FNonAbandonableTask
{
public:
	FFileSortAndFilterTask(FName InSortedColumn, EColumnSortMode::Type InSortMode, TSharedPtr<SPakFileView> InPakFileView)
		: CurrentSortedColumn(InSortedColumn)
		, CurrentSortMode(InSortMode)
		, CurrentSearchText(TEXT(""))
		, CurrentLoadGuid(TEXT(""))
		, WeakPakFileView(InPakFileView)
	{

	}

	void DoWork();
	void SetWorkInfo(FName InSortedColumn, EColumnSortMode::Type InSortMode, const FString& InSearchText, const FString& InLoadGuid, const TMap<FName, bool>& InClassFilterMap);
	FOnSortAndFilterFinished& GetOnSortAndFilterFinishedDelegate() { return OnWorkFinished; }

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(STAT_FFileSortAndFilterTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	void RetriveResult(TArray<FPakFileEntryPtr>& OutResult);

protected:
	FName CurrentSortedColumn;
	EColumnSortMode::Type CurrentSortMode;
	FString CurrentSearchText;
	FString CurrentLoadGuid;

	/** Shared pointer to parent PakFileView widget. Used for accesing the cache and to check if cancel is requested. */
	TWeakPtr<SPakFileView> WeakPakFileView;

	FOnSortAndFilterFinished OnWorkFinished;

	FCriticalSection CriticalSection;
	TArray<FPakFileEntryPtr> Result;

	TMap<FName, bool> ClassFilterMap;
};