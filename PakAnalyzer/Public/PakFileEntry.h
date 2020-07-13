#pragma once

#include "CoreMinimal.h"

struct FPakEntry;
struct FPakInfo;

struct FPakFileEntry : TSharedFromThis<FPakFileEntry>
{
	const FPakEntry* PakEntry = nullptr;
	FString Filename;
	FString Path;
};

typedef TSharedPtr<FPakFileEntry> FPakFileEntryPtr;

struct FPakTreeEntry : public FPakFileEntry
{
	bool bIsDirectory;
	TArray<TSharedPtr<FPakTreeEntry>> Children;
};

typedef TSharedPtr<FPakTreeEntry> FPakTreeEntryPtr;

struct FPakFileSumary
{
	const FPakInfo* PakInfo = nullptr;
	FString MountPoint;
};
