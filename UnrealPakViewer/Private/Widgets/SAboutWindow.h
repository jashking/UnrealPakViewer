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

protected:
	bool LoadTexture(const FString& ResourcePath, uint32& OutWidth, uint32& OutHeight, TArray<uint8>& OutDecodedImage);
	bool LoadTexture(const TArray<uint8>& RawFileData, uint32& OutWidth, uint32& OutHeight, TArray<uint8>& OutDecodedImage);

	void OnDonateAreaExpansionChanged(bool bExpand);
};