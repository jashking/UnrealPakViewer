// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UnrealPakViewerMain.h"
#include "UnixCommonStartup.h"

/**
 * main(), called when the application is started
 */
int main(int argc, const char *argv[])
{
	return CommonUnixMain(argc, argv, &UnrealPakViewerMain);
}
