#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Implements the Pak Info window. */
class SPakTreeView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SPakTreeView();

	/** Virtual destructor. */
	virtual ~SPakTreeView();

	SLATE_BEGIN_ARGS(SPakTreeView) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);
};
