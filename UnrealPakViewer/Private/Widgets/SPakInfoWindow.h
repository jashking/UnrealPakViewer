#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Implements the Pak Info window. */
class SPakInfoWindow : public SCompoundWidget
{
public:
	/** Default constructor. */
	SPakInfoWindow();

	/** Virtual destructor. */
	virtual ~SPakInfoWindow();

	SLATE_BEGIN_ARGS(SPakInfoWindow) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);
};
