#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Implements the Pak Info window. */
class SPakDetailView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SPakDetailView();

	/** Virtual destructor. */
	virtual ~SPakDetailView();

	SLATE_BEGIN_ARGS(SPakDetailView) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);
};
