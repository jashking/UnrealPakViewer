#pragma once

struct FPakEntry;

struct FPakFileEntry
{
	const FPakEntry* PakEntry = nullptr;
	FString Filename;
	FString Path;
};