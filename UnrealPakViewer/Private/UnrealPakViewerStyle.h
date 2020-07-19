#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class  FUnrealPakViewerStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static FName GetStyleSetName();

	static const ISlateStyle& Get();

protected:
	static TSharedRef<FSlateStyleSet> Create();

protected:
	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
