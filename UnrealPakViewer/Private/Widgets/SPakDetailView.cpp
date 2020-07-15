#include "SPakDetailView.h"

#include "EditorStyle.h"
#include "IPlatformFilePak.h"
#include "Styling/CoreStyle.h"

#include "PakAnalyzerModule.h"
#include "SKeyValueRow.h"

#define LOCTEXT_NAMESPACE "SPakInfoView"

// TODO: CompressionMethod
// TODO: IndexSize
// TODO: PakHeaderSize
// TODO: Version
// TODO: Compression Array

SPakDetailView::SPakDetailView()
{

}

SPakDetailView::~SPakDetailView()
{

}

void SPakDetailView::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(2.0f)
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_Path", "Pak Path:")).ValueText(this, &SPakDetailView::GetPakPath)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_MountPoint", "Mount Point:")).ValueText(this, &SPakDetailView::GetPakMountPoint)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_Version", "Pak Version:")).ValueText(this, &SPakDetailView::GetPakVersion)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_FileSize", "Pak File Size:")).ValueText(this, &SPakDetailView::GetPakFileSize).ValueToolTipText(this, &SPakDetailView::GetPakFileSizeToolTip)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_FileCount", "Pak File Count:")).ValueText(this, &SPakDetailView::GetPakFileCount)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_HeaderSize", "Pak Header Size:")).ValueText(this, &SPakDetailView::GetPakHeaderSize).ValueToolTipText(this, &SPakDetailView::GetPakHeaderSizeToolTip)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_IndexSize", "Pak Index Size:")).ValueText(this, &SPakDetailView::GetPakIndexSize).ValueToolTipText(this, &SPakDetailView::GetPakIndexSizeToolTip)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_IndexHash", "Pak Index Hash:")).ValueText(this, &SPakDetailView::GetPakFileIndexHash)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_IndexEncrypted", "Pak Index Is Encrypted:")).ValueText(this, &SPakDetailView::GetPakFileIndexIsEncrypted)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_FileContentSize", "Pak File Content Size:")).ValueText(this, &SPakDetailView::GetPakFileContentSize).ValueToolTipText(this, &SPakDetailView::GetPakFileContentSizeToolTip)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 0.f)
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Detail_View_Pak_CompressionMethods", "Pak Compression Methods:")).ValueText(this, &SPakDetailView::GetPakFileEncryptionMethods)
			]
		]

		//+ SHorizontalBox::Slot()
		//.FillWidth(1.f)
		//.Padding(2.0f)
		//[
		//]
	];
}

FORCEINLINE FText SPakDetailView::GetPakPath() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	return PakAnalyzer.IsValid() ? FText::FromString(PakAnalyzer->GetPakFileSumary().PakFilePath) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakMountPoint() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	return PakAnalyzer.IsValid() ? FText::FromString(PakAnalyzer->GetPakFileSumary().MountPoint) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakVersion() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const FPakInfo* PakInfo = PakAnalyzer.IsValid() ? PakAnalyzer->GetPakFileSumary().PakInfo : nullptr;

	return PakInfo ? FText::AsNumber(PakInfo->Version) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakFileSize() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	return PakAnalyzer.IsValid() ? FText::AsMemory(PakAnalyzer->GetPakFileSumary().PakFileSize, EMemoryUnitStandard::IEC) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakFileSizeToolTip() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	return PakAnalyzer.IsValid() ? FText::AsNumber(PakAnalyzer->GetPakFileSumary().PakFileSize) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakFileCount() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	return PakAnalyzer.IsValid() ? FText::AsNumber(PakAnalyzer->GetFileCount()) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakHeaderSize() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const FPakInfo* PakInfo = PakAnalyzer.IsValid() ? PakAnalyzer->GetPakFileSumary().PakInfo : nullptr;

	return PakInfo ? FText::AsMemory(PakAnalyzer->GetPakFileSumary().PakFileSize - PakInfo->IndexOffset - PakInfo->IndexSize, EMemoryUnitStandard::IEC) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakHeaderSizeToolTip() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const FPakInfo* PakInfo = PakAnalyzer.IsValid() ? PakAnalyzer->GetPakFileSumary().PakInfo : nullptr;

	return PakInfo ? FText::AsNumber(PakAnalyzer->GetPakFileSumary().PakFileSize - PakInfo->IndexOffset - PakInfo->IndexSize) : FText();

}

FORCEINLINE FText SPakDetailView::GetPakIndexSize() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const FPakInfo* PakInfo = PakAnalyzer.IsValid() ? PakAnalyzer->GetPakFileSumary().PakInfo : nullptr;

	return PakInfo ? FText::AsMemory(PakInfo->IndexSize, EMemoryUnitStandard::IEC) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakIndexSizeToolTip() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const FPakInfo* PakInfo = PakAnalyzer.IsValid() ? PakAnalyzer->GetPakFileSumary().PakInfo : nullptr;

	return PakInfo ? FText::AsNumber(PakInfo->IndexSize) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakFileIndexHash() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const FPakInfo* PakInfo = PakAnalyzer.IsValid() ? PakAnalyzer->GetPakFileSumary().PakInfo : nullptr;
	
	return PakInfo ? FText::FromString(LexToString(PakInfo->IndexHash)) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakFileIndexIsEncrypted() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const FPakInfo* PakInfo = PakAnalyzer.IsValid() ? PakAnalyzer->GetPakFileSumary().PakInfo : nullptr;

	return PakInfo ? FText::FromString(PakInfo->bEncryptedIndex ? TEXT("True") : TEXT("False")) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakFileContentSize() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const FPakInfo* PakInfo = PakAnalyzer.IsValid() ? PakAnalyzer->GetPakFileSumary().PakInfo : nullptr;

	return PakInfo ? FText::AsMemory(PakInfo->IndexOffset, EMemoryUnitStandard::IEC) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakFileContentSizeToolTip() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const FPakInfo* PakInfo = PakAnalyzer.IsValid() ? PakAnalyzer->GetPakFileSumary().PakInfo : nullptr;

	return PakInfo ? FText::AsNumber(PakInfo->IndexOffset) : FText();
}

FORCEINLINE FText SPakDetailView::GetPakFileEncryptionMethods() const
{
	TSharedPtr<IPakAnalyzer> PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	const FPakInfo* PakInfo = PakAnalyzer.IsValid() ? PakAnalyzer->GetPakFileSumary().PakInfo : nullptr;

	TArray<FString> Methods;
	if (PakInfo)
	{
		for (const FName& Name : PakInfo->CompressionMethods)
		{
			Methods.Add(Name.ToString());
		}
	}

	return PakInfo ? FText::FromString(FString::Join(Methods, TEXT(", "))) : FText();
}

#undef LOCTEXT_NAMESPACE
