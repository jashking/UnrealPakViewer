#include "FileSortAndFilter.h"

#include "Misc/ScopeLock.h"
#include "PakAnalyzerModule.h"
#include "ViewModels/FileColumn.h"
#include "Widgets/SPakFileView.h"

void FFileSortAndFilterTask::DoWork()
{
	TSharedPtr<SPakFileView> PakFileViewPin = WeakPakFileView.Pin();
	if (!PakFileViewPin.IsValid())
	{
		return;
	}

	TArray<FPakFileEntryPtr> FilterResult;
	IPakAnalyzerModule::Get().GetPakAnalyzer()->GetFiles(CurrentSearchText, ClassFilterMap, IndexFilterMap, FilterResult);

	const FFileColumn* Column = PakFileViewPin->FindCoulum(CurrentSortedColumn);
	if (!Column)
	{
		return;
	}

	if (!Column->CanBeSorted())
	{
		return;
	}

	if (CurrentSortMode == EColumnSortMode::Ascending)
	{
		FilterResult.Sort(Column->GetAscendingCompareDelegate());
	}
	else
	{
		FilterResult.Sort(Column->GetDescendingCompareDelegate());
	}

	{
		FScopeLock Lock(&CriticalSection);
		Result = MoveTemp(FilterResult);
	}

	OnWorkFinished.ExecuteIfBound(CurrentSortedColumn, CurrentSortMode, CurrentSearchText);
}

void FFileSortAndFilterTask::SetWorkInfo(FName InSortedColumn, EColumnSortMode::Type InSortMode, const FString& InSearchText, const TMap<FName, bool>& InClassFilterMap, const TMap<int32, bool>& InIndexFilterMap)
{
	CurrentSortedColumn = InSortedColumn;
	CurrentSortMode = InSortMode;
	CurrentSearchText = InSearchText;
	ClassFilterMap = InClassFilterMap;
	IndexFilterMap = InIndexFilterMap;
}

void FFileSortAndFilterTask::RetriveResult(TArray<FPakFileEntryPtr>& OutResult)
{
	FScopeLock Lock(&CriticalSection);
	OutResult = MoveTemp(Result);
}
