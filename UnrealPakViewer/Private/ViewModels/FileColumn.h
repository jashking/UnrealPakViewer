#pragma once

#include "CoreMinimal.h"

#include "PakFileEntry.h"

enum class EFileColumnFlags : uint32
{
	None = 0,

	ShouldBeVisible = (1 << 0),
	CanBeHidden = (1 << 1),
	CanBeFiltered = (1 << 2),
};
ENUM_CLASS_FLAGS(EFileColumnFlags);

class FFileColumn
{
public:
	typedef TFunction<bool(const FPakFileEntryPtr& A, const FPakFileEntryPtr& B)> FFileCompareFunc;

	static const FName NameColumnName;
	static const FName PathColumnName;
	static const FName ClassColumnName;
	static const FName OffsetColumnName;
	static const FName SizeColumnName;
	static const FName CompressedSizeColumnName;
	static const FName CompressionBlockCountColumnName;
	static const FName CompressionBlockSizeColumnName;
	static const FName CompressionMethodColumnName;
	static const FName SHA1ColumnName;
	static const FName IsEncryptedColumnName;

	FFileColumn() = delete;
	FFileColumn(int32 InIndex, const FName InId, const FText& InTitleName, const FText& InDescription, float InInitialWidth, const EFileColumnFlags& InFlags, FFileCompareFunc InAscendingCompareDelegate = nullptr, FFileCompareFunc InDescendingCompareDelegate = nullptr)
		: Index(InIndex)
		, Id(InId)
		, TitleName(InTitleName)
		, Description(InDescription)
		, InitialWidth(InInitialWidth)
		, Flags(InFlags)
		, AscendingCompareDelegate(InAscendingCompareDelegate)
		, DescendingCompareDelegate(InDescendingCompareDelegate)
		, bIsVisible(EnumHasAnyFlags(Flags, EFileColumnFlags::ShouldBeVisible))
	{
	}

	int32 GetIndex() const { return Index; }
	const FName& GetId() const { return Id; }
	const FText& GetTitleName() const { return TitleName; }
	const FText& GetDescription() const { return Description; }

	bool IsVisible() const { return bIsVisible; }
	void Show() { bIsVisible = true; }
	void Hide() { bIsVisible = false; }
	void ToggleVisibility() { bIsVisible = !bIsVisible; }
	void SetVisibilityFlag(bool bOnOff) { bIsVisible = bOnOff; }

	float GetInitialWidth() const { return InitialWidth; }

	/** Whether this column should be initially visible. */
	bool ShouldBeVisible() const { return EnumHasAnyFlags(Flags, EFileColumnFlags::ShouldBeVisible); }

	/** Whether this column can be hidden. */
	bool CanBeHidden() const { return EnumHasAnyFlags(Flags, EFileColumnFlags::CanBeHidden); }

	/** Whether this column can be used for filtering displayed results. */
	bool CanBeFiltered() const { return EnumHasAnyFlags(Flags, EFileColumnFlags::CanBeFiltered); }

	/** Whether this column can be used for sort displayed results. */
	bool CanBeSorted() const { return AscendingCompareDelegate && DescendingCompareDelegate; }

	void SetAscendingCompareDelegate(FFileCompareFunc InCompareDelegate) { AscendingCompareDelegate = InCompareDelegate; }
	void SetDescendingCompareDelegate(FFileCompareFunc InCompareDelegate) { DescendingCompareDelegate = InCompareDelegate; }

	FFileCompareFunc GetAscendingCompareDelegate() const { return AscendingCompareDelegate; }
	FFileCompareFunc GetDescendingCompareDelegate() const { return DescendingCompareDelegate; }

protected:
	int32 Index;
	FName Id;
	FText TitleName;
	FText Description;
	float InitialWidth;
	EFileColumnFlags Flags;
	FFileCompareFunc AscendingCompareDelegate;
	FFileCompareFunc DescendingCompareDelegate;

	bool bIsVisible;
};