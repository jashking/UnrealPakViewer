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

	const TArray<FPakFileEntryPtr>& AllFiles = IPakAnalyzerModule::Get().GetPakAnalyzer()->GetFiles();
	for (int32 Index = 0; Index < AllFiles.Num(); ++Index)
	{
		FileCache.Add(AllFiles[Index]);
	}

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

	PakFileViewPin->RefreshFileCache(FileCache);
}

void FFileSortAndFilterTask::SetSortInfo(FName InSortedColumn, EColumnSortMode::Type InSortMode)
{
	CurrentSortedColumn = InSortedColumn;
	CurrentSortMode = InSortMode;
}

