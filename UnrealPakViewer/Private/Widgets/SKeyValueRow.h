#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SKeyValueRow : public SCompoundWidget
{
public:
	/** Default constructor. */
	SKeyValueRow() {}

	/** Virtual destructor. */
	virtual ~SKeyValueRow() {}

	SLATE_BEGIN_ARGS(SKeyValueRow)
		: _KeyStretchCoefficient(0.4f)
	{}
	SLATE_ATTRIBUTE(FText, KeyText)
	SLATE_ATTRIBUTE(FText, KeyToolTipText)
	SLATE_ATTRIBUTE(FText, ValueText)
	SLATE_ATTRIBUTE(FText, ValueToolTipText)
	SLATE_ATTRIBUTE(float, KeyStretchCoefficient)
	SLATE_END_ARGS()

	/** Constructs this widget. */
	void Construct(const FArguments& InArgs);

protected:
	TAttribute<FText> KeyText;
	TAttribute<FText> KeyToolTipText;
	TAttribute<FText> ValueText;
	TAttribute<FText> ValueToolTipText;
	TAttribute<float> KeyStretchCoefficient;
};