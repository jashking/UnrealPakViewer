#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"

DECLARE_DELEGATE_OneParam(FOnConfirm, const FString&);

class SKeyInputWindow : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SKeyInputWindow)
	{
	}

	SLATE_EVENT(FOnConfirm, OnConfirm)
	SLATE_ATTRIBUTE(FString, PakPath)
	SLATE_ATTRIBUTE(FGuid, PakGuid)
	SLATE_END_ARGS()

	SKeyInputWindow();
	virtual	~SKeyInputWindow();

	/** Widget constructor */
	void Construct(const FArguments& Args);

protected:
	FReply OnConfirmButtonClicked();

protected:
	FOnConfirm OnConfirm;
	TAttribute<FString> PakPath;
	TAttribute<FGuid> PakGuid;

	TSharedPtr<class SEditableTextBox> EncryptionKeyBox;
};