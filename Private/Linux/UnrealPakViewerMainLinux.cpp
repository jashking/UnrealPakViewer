// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "UnrealPakViewer.h"
#include <locale.h>

/**
 * main(), called when the application is started
 */
int main(int argc, const char *argv[])
{
	FString SavedCommandLine;

	setlocale(LC_CTYPE, "");
	for (int32 Option = 1; Option < argc; Option++)
	{
		SavedCommandLine += TEXT(" ");
		SavedCommandLine += UTF8_TO_TCHAR(argv[Option]);	// note: technically it depends on locale
	}

	// Run the app
	RunUnrealPakViewer(*SavedCommandLine);

	return 0;
}
