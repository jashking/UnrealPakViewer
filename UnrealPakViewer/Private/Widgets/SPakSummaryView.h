#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "PakFileEntry.h"

/** Implements the Pak Info window. */
class SPakSummaryView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SPakSummaryView();

	/** Virtual destructor. */
	virtual ~SPakSummaryView();

	SLATE_BEGIN_ARGS(SPakSummaryView) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);

protected:
	FORCEINLINE FText GetAssetRegistryPath() const;

	void OnLoadPakFinished();
	FReply OnLoadAssetRegistry();

	TSharedRef<ITableRow> OnGenerateSummaryRow(FPakFileSumaryPtr InSummary, const TSharedRef<class STableViewBase>& OwnerTable);

protected:
	TSharedPtr<SListView<FPakFileSumaryPtr>> SummaryListView;
	TArray<FPakFileSumaryPtr> Summaries;
};
