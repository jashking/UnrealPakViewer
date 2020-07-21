#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"

class SAboutWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SAboutWindow)
	{
	}
	SLATE_END_ARGS()

	SAboutWindow();
	virtual	~SAboutWindow();

	/** Widget constructor */
	void Construct(const FArguments& Args);
};