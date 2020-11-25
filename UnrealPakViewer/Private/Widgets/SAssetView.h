#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SAssetView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SAssetView();

	/** Virtual destructor. */
	virtual ~SAssetView();

	SLATE_BEGIN_ARGS(SAssetView) {}
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);
};
