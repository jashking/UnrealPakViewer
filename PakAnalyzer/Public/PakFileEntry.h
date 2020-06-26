#pragma once

#include "CoreMinimal.h"

struct FPakEntry;

struct FPakFileEntry
{
	const FPakEntry* PakEntry = nullptr;
	FString Filename;
	FString Path;
};

typedef TSharedPtr<FPakFileEntry> FPakFileEntryPtr;
