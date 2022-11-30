#include "SPakSummaryView.h"

#include "DesktopPlatformModule.h"
//#include "EditorStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "IPlatformFilePak.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Views/STableRow.h"

#include "CommonDefines.h"
#include "PakAnalyzerModule.h"
#include "SKeyValueRow.h"
#include "ViewModels/WidgetDelegates.h"

#define LOCTEXT_NAMESPACE "SPakSummaryView"

class SSummaryRow : public SMultiColumnTableRow<FPakFileSumaryPtr>
{
	SLATE_BEGIN_ARGS(SSummaryRow) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, FPakFileSumaryPtr InSummary, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		if (!InSummary.IsValid())
		{
			return;
		}

		WeakSummary = MoveTemp(InSummary);

		SMultiColumnTableRow<FPakFileSumaryPtr>::Construct(FSuperRowType::FArguments().Padding(FMargin(0.f, 2.f)), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		static const float LeftMargin = 4.f;

		FPakFileSumaryPtr Summary = WeakSummary.Pin();
		if (!Summary.IsValid())
		{
			return SNew(STextBlock).Text(LOCTEXT("NullColumn", "Null")).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}

		TSharedRef<SWidget> RowContent = SNullWidget::NullWidget;

		if (ColumnName == "Name")
		{
			RowContent = SNew(STextBlock).Text(FText::FromString(FPaths::GetCleanFilename(Summary->PakFilePath))).ToolTipText(FText::FromString(Summary->PakFilePath));
		}
		else if (ColumnName == "MountPoint")
		{
			RowContent = SNew(STextBlock).Text(FText::FromString(Summary->MountPoint)).ToolTipText(FText::FromString(Summary->MountPoint)).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else if (ColumnName == "Version")
		{
			RowContent = SNew(STextBlock).Text(FText::AsNumber(Summary->PakInfo.Version)).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "FileCount")
		{
			RowContent = SNew(STextBlock).Text(FText::AsNumber(Summary->FileCount)).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "PakSize")
		{
			RowContent = SNew(STextBlock).Text(FText::AsMemory(Summary->PakFileSize, EMemoryUnitStandard::IEC)).ToolTipText(FText::AsNumber(Summary->PakFileSize)).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "ContentSize")
		{
			RowContent = SNew(STextBlock).Text(FText::AsMemory(Summary->PakInfo.IndexOffset, EMemoryUnitStandard::IEC)).ToolTipText(FText::AsNumber(Summary->PakInfo.IndexOffset)).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "HeaderSize")
		{
			RowContent = SNew(STextBlock).Text(FText::AsMemory(Summary->PakFileSize - Summary->PakInfo.IndexOffset - Summary->PakInfo.IndexSize, EMemoryUnitStandard::IEC)).ToolTipText(FText::AsNumber(Summary->PakFileSize - Summary->PakInfo.IndexOffset - Summary->PakInfo.IndexSize)).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "IndexSize")
		{
			RowContent = SNew(STextBlock).Text(FText::AsMemory(Summary->PakInfo.IndexSize, EMemoryUnitStandard::IEC)).ToolTipText(FText::AsNumber(Summary->PakInfo.IndexSize)).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "IndexHash")
		{
			RowContent = SNew(STextBlock).Text(FText::FromString(LexToString(Summary->PakInfo.IndexHash)));
		}
		else if (ColumnName == "IndexEncrypted")
		{
			RowContent = SNew(STextBlock).Text(FText::FromString(Summary->PakInfo.bEncryptedIndex ? TEXT("True") : TEXT("False"))).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "CompressionMethods")
		{
			RowContent = SNew(STextBlock).Text(FText::FromString(Summary->CompressionMethods)).ToolTipText(FText::FromString(Summary->CompressionMethods));
		}
		else if (ColumnName == "DecryptAES")
		{
			RowContent = SNew(STextBlock).Text(FText::FromString(Summary->DecryptAESKeyStr)).ToolTipText(FText::FromString(Summary->DecryptAESKeyStr)).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else
		{
			RowContent = SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column")).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}

		return SNew(SBox).VAlign(VAlign_Center)
			[
				RowContent
			];
	}

protected:
	TWeakPtr<FPakFileSumary> WeakSummary;
};

SPakSummaryView::SPakSummaryView()
{
	FPakAnalyzerDelegates::OnPakLoadFinish.AddRaw(this, &SPakSummaryView::OnLoadPakFinished);
}

SPakSummaryView::~SPakSummaryView()
{
	FPakAnalyzerDelegates::OnPakLoadFinish.RemoveAll(this);
}

void SPakSummaryView::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.AreaTitle(LOCTEXT("PakSumary", "Pak Summary"))
			.BodyContent()
			[
				SAssignNew(SummaryListView, SListView<FPakFileSumaryPtr>)
				.ItemHeight(25.f)
				.SelectionMode(ESelectionMode::Single)
				.ListItemsSource(&Summaries)
				.OnGenerateRow(this, &SPakSummaryView::OnGenerateSummaryRow)
				.HeaderRow
				(
					SNew(SHeaderRow).Visibility(EVisibility::Visible)

					+ SHeaderRow::Column(FName("Name"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_Name", "Name"))

					+ SHeaderRow::Column(FName("MountPoint"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_MountPoint", "Mount Point"))

					+ SHeaderRow::Column(FName("Version"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_Version", "Version"))

					+ SHeaderRow::Column(FName("FileCount"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_FileCount", "File Count"))

					+ SHeaderRow::Column(FName("PakSize"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_PakSize", "Pak Size"))

					+ SHeaderRow::Column(FName("ContentSize"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_ContentSize", "Content Size"))

					+ SHeaderRow::Column(FName("HeaderSize"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_HeaderSize", "Header Size"))

					+ SHeaderRow::Column(FName("IndexSize"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_IndexSize", "Index Size"))

					+ SHeaderRow::Column(FName("IndexHash"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_IndexHash", "Index Hash"))

					+ SHeaderRow::Column(FName("IndexEncrypted"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_IndexEncrypted", "Index Encrypted"))

					+ SHeaderRow::Column(FName("CompressionMethods"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_CompressionMethods", "Compression Methods"))

					+ SHeaderRow::Column(FName("DecryptAES"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Package_Summary_DecryptAES", "Decrypt AES"))
				)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().AutoWidth().Padding(2.f, 0.f, 5.f, 0.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("AssetRegistryText", "AssetRegistry:")).ColorAndOpacity(FLinearColor::Green).ShadowOffset(FVector2D(1.f, 1.f))
			]

			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 5.f, 0.f)
			[
				SNew(SEditableTextBox).IsReadOnly(true).Text(this, &SPakSummaryView::GetAssetRegistryPath)
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 0.f, 0.f).VAlign(VAlign_Center)
			[
				SNew(SButton).Text(LOCTEXT("LoadAssetRegistryText", "Load Asset Registry")).OnClicked(this, &SPakSummaryView::OnLoadAssetRegistry).ToolTipText(LOCTEXT("LoadAssetRegistryTipText", "Default in the path: [Your Project Path]/Saved/Cooked/[PLATFORM]/ProjectName"))
			]
		]
	];
}

FORCEINLINE FText SPakSummaryView::GetAssetRegistryPath() const
{
	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();

	return PakAnalyzer ? FText::FromString(PakAnalyzer->GetAssetRegistryPath()) : FText();
}

void SPakSummaryView::OnLoadPakFinished()
{
	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	if (PakAnalyzer)
	{
		Summaries = PakAnalyzer->GetPakFileSumary();

		if (SummaryListView.IsValid())
		{
			SummaryListView->RebuildList();
		}
	}
}

FReply SPakSummaryView::OnLoadAssetRegistry()
{
	IPakAnalyzer* PakAnalyzer = IPakAnalyzerModule::Get().GetPakAnalyzer();
	if (!PakAnalyzer)
	{
		return FReply::Handled();
	}

	TArray<FString> OutFiles;
	bool bOpened = false;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FSlateApplication::Get().CloseToolTip();

		bOpened = DesktopPlatform->OpenFileDialog
		(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			LOCTEXT("LoadAssetRegistry_FileDesc", "Open asset registry file...").ToString(),
			TEXT(""),
			TEXT(""),
			LOCTEXT("LoadAssetRegistry_FileFilter", "Asset regirstry files (*.bin)|*.bin|All files (*.*)|*.*").ToString(),
			EFileDialogFlags::None,
			OutFiles
		);
	}

	if (bOpened && OutFiles.Num() > 0)
	{
		if (PakAnalyzer->LoadAssetRegistry(OutFiles[0]))
		{
			FWidgetDelegates::GetOnLoadAssetRegistryFinishedDelegate().Broadcast();
		}
	}
	return FReply::Handled();
}

TSharedRef<ITableRow> SPakSummaryView::OnGenerateSummaryRow(FPakFileSumaryPtr InSummary, const TSharedRef<class STableViewBase>& OwnerTable)
{
	return SNew(SSummaryRow, InSummary, OwnerTable);
}

#undef LOCTEXT_NAMESPACE
