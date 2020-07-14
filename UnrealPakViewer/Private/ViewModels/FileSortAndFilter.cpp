#include "FileSortAndFilter.h"

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

	TArray<FPakFileEntryPtr> FileCache;
	IPakAnalyzerModule::Get().GetPakAnalyzer()->GetFiles(CurrentSearchText, FileCache);

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
		FileCache.Sort(Column->GetAscendingCompareDelegate());
	}
	else
	{
		FileCache.Sort(Column->GetDescendingCompareDelegate());
	}

	OnWorkFinished.ExecuteIfBound(FileCache, CurrentSortedColumn, CurrentSortMode, CurrentSearchText, CurrentLoadGuid);
}

void FFileSortAndFilterTask::SetWorkInfo(FName InSortedColumn, EColumnSortMode::Type InSortMode, const FString& InSearchText, const FString& InLoadGuid)
{
	CurrentSortedColumn = InSortedColumn;
	CurrentSortMode = InSortMode;
	CurrentSearchText = InSearchText;
	CurrentLoadGuid = InLoadGuid;
}
