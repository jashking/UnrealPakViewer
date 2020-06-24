#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Implements the Pak Info window. */
class SPakInfoView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SPakInfoView();

	/** Virtual destructor. */
	virtual ~SPakInfoView();

	SLATE_BEGIN_ARGS(SPakInfoView) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);
};
