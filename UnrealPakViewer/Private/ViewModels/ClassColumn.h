#pragma once

#include "CoreMinimal.h"

#include "PakFileEntry.h"

class FClassColumn
{
public:
	static const FName ClassColumnName;
	static const FName FileCountColumnName;
	static const FName SizeColumnName;
	static const FName CompressedSizeColumnName;
	static const FName PercentOfTotalColumnName;
	static const FName PercentOfParentColumnName;

	FClassColumn() = delete;
	FClassColumn(int32 InIndex, const FName InId, const FText& InTitleName, const FText& InDescription, float InInitialWidth)
		: Index(InIndex)
		, Id(InId)
		, TitleName(InTitleName)
		, Description(InDescription)
		, InitialWidth(InInitialWidth)
	{
	}

	int32 GetIndex() const { return Index; }
	const FName& GetId() const { return Id; }
	const FText& GetTitleName() const { return TitleName; }
	const FText& GetDescription() const { return Description; }

	float GetInitialWidth() const { return InitialWidth; }

	static FLinearColor GetColorByClass(const TCHAR* const InClassName)
	{
		uint32 Hash = 0;
		for (const TCHAR* c = InClassName; *c; ++c)
		{
			Hash = (Hash + *c) * 0x2c2c57ed;
		}

		// Divided by 128.0 in order to force bright colors.
		return FLinearColor(((Hash >> 16) & 0xFF) / 128.0f, ((Hash >> 8) & 0xFF) / 128.0f, (Hash & 0xFF) / 128.0f, 1.0f);
	}

protected:
	int32 Index;
	FName Id;
	FText TitleName;
	FText Description;
	float InitialWidth;
};