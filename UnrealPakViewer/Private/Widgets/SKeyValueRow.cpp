#include "SKeyValueRow.h"

void SKeyValueRow::Construct(const FArguments& InArgs)
{
	KeyText = InArgs._KeyText;
	KeyToolTipText = InArgs._KeyToolTipText;
	ValueText = InArgs._ValueText;
	ValueToolTipText = InArgs._ValueToolTipText;
	KeyStretchCoefficient = InArgs._KeyStretchCoefficient;

	TSharedRef<SWidget> KeyContent = InArgs._KeyContent.Widget;
	if (KeyContent == SNullWidget::NullWidget)
	{
		KeyContent = SNew(STextBlock).Text(KeyText).ToolTipText(KeyToolTipText).ColorAndOpacity(FLinearColor::Green).ShadowOffset(FVector2D(1.f, 1.f));
	}

	TSharedRef<SWidget> ValueContent = InArgs._ValueContent.Widget;
	if (ValueContent == SNullWidget::NullWidget)
	{
		ValueContent = SNew(STextBlock).Text(ValueText).ToolTipText(ValueToolTipText);
	}

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(KeyStretchCoefficient)
		//.Padding(2.0f)
		[
			KeyContent
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		//.Padding(2.0f)
		[
			ValueContent
		]
	];
}
