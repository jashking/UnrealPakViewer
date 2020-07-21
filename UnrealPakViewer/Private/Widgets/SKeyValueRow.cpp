#include "SKeyValueRow.h"

void SKeyValueRow::Construct(const FArguments& InArgs)
{
	KeyText = InArgs._KeyText;
	KeyToolTipText = InArgs._KeyToolTipText;
	ValueText = InArgs._ValueText;
	ValueToolTipText = InArgs._ValueToolTipText;
	KeyStretchCoefficient = InArgs._KeyStretchCoefficient;

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(KeyStretchCoefficient)
		//.Padding(2.0f)
		[
			SNew(STextBlock).Text(KeyText).ToolTipText(KeyToolTipText).ColorAndOpacity(FLinearColor::Green).ShadowOffset(FVector2D(1.f, 1.f))
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		//.Padding(2.0f)
		[
			SNew(STextBlock).Text(ValueText).ToolTipText(ValueToolTipText)
		]
	];
}
