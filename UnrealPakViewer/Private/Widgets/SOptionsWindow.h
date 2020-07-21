#pragma once

#include "CoreMinimal.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SWindow.h"

class SOptionsWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SOptionsWindow)
	{
	}
	SLATE_END_ARGS()

	SOptionsWindow();
	virtual	~SOptionsWindow();

	/** Widget constructor */
	void Construct(const FArguments& Args);

protected:
	FReply OnApply();
	FReply OnCancel();

protected:
	TSharedPtr<SSpinBox<int32>> ThreadCountBox;
};