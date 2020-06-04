// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FUnrealPakViewerApplication
{
public:

	/** Executes the application. */
	static void Exec();

protected:

	/**
	 * Initializes the application.
	 *
	 */
	static void InitializeApplication();

	/**
	 * Shuts down the application.
	 *
	 */
	static void ShutdownApplication();
};