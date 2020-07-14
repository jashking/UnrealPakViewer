#include "SKeyValueRow.h"

void SKeyValueRow::Construct(const FArguments& InArgs)
{
	KeyText = InArgs._KeyText;
	KeyToolTipText = InArgs._KeyToolTipText;
	ValueText = InArgs._ValueText;
	ValueToolTipText = InArgs._ValueToolTipText;

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(0.4f)
		.Padding(2.0f)
		[
			SNew(STextBlock).Text(KeyText).ToolTipText(KeyToolTipText)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(2.0f)
		[
			SNew(STextBlock).Text(ValueText).ToolTipText(ValueToolTipText)
		]
	];
}

