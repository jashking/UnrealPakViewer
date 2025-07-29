#include "SAssetSummaryView.h"

//#include "EditorStyle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

#include "SKeyValueRow.h"

#include "UnrealPakViewerStyle.h"

#define LOCTEXT_NAMESPACE "SAssetSummaryView"

#define DEFINE_GET_MEMBER_FUNCTION_NUMBER(MemberName) \
	FORCEINLINE FText SAssetSummaryView::Get##MemberName() const \
	{ \
		return ViewingPackage.IsValid() ? FText::AsNumber(ViewingPackage->AssetSummary->PackageSummary.MemberName) : FText(); \
	}

////////////////////////////////////////////////////////////////////////////////////////////////////
// SImportObjectRow
////////////////////////////////////////////////////////////////////////////////////////////////////

class SImportObjectRow : public SMultiColumnTableRow<FObjectImportPtrType>
{
	SLATE_BEGIN_ARGS(SImportObjectRow) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, FObjectImportPtrType InObject, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		if (!InObject.IsValid())
		{
			return;
		}

		WeakObject = MoveTemp(InObject);

		SMultiColumnTableRow<FObjectImportPtrType>::Construct(FSuperRowType::FArguments().Padding(FMargin(0.f, 2.f)), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		static const float LeftMargin = 4.f;

		FObjectImportPtrType Object = WeakObject.Pin();
		if (!Object.IsValid())
		{
			return SNew(STextBlock).Text(LOCTEXT("NullColumn", "Null")).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}

		if (ColumnName == "Index")
		{
			return SNew(STextBlock).Text(FText::AsNumber(Object->Index));
		}
		else if (ColumnName == "ClassPackage")
		{
			return SNew(STextBlock).Text(FText::FromName(Object->ClassPackage)).ToolTipText(FText::FromName(Object->ClassPackage)).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else if (ColumnName == "ClassName")
		{
			return SNew(STextBlock).Text(FText::FromName(Object->ClassName)).ToolTipText(FText::FromName(Object->ClassName)).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else if (ColumnName == "ObjectName")
		{
			return SNew(STextBlock).Text(FText::FromName(Object->ObjectName)).ToolTipText(FText::FromName(Object->ObjectName)).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else if (ColumnName == "FullPath")
		{
			return SNew(STextBlock).Text(FText::FromName(Object->ObjectPath)).ToolTipText(FText::FromName(Object->ObjectPath)).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else
		{
			return SNew(STextBlock).Text(LOCTEXT("UnknownColumn", "Unknown Column")).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
	}

protected:
	TWeakPtr<FObjectImportEx> WeakObject;
};

class SExportObjectRow : public SMultiColumnTableRow<FObjectExportPtrType>
{
	SLATE_BEGIN_ARGS(SExportObjectRow) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, FObjectExportPtrType InObject, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		if (!InObject.IsValid())
		{
			return;
		}

		for (FPackageInfoPtr Dependency : InObject->DependencyList)
		{
			Dependencies.Add(MakeShared<FName>(*FString::Printf(TEXT("%s: %s"), *Dependency->ExtraInfo.ToString(), *Dependency->PackageName.ToString())));
		}

		WeakObject = MoveTemp(InObject);

		SMultiColumnTableRow<FObjectExportPtrType>::Construct(FSuperRowType::FArguments().Padding(FMargin(0.f, 2.f)), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		static const float LeftMargin = 4.f;

		FObjectExportPtrType Object = WeakObject.Pin();
		if (!Object.IsValid())
		{
			return SNew(STextBlock).Text(LOCTEXT("NullColumn", "Null")).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}

		TSharedRef<SWidget> RowContent = SNullWidget::NullWidget;

		if (ColumnName == "Index")
		{
			RowContent = SNew(STextBlock).Text(FText::AsNumber(Object->Index));
		}
		else if (ColumnName == "ObjectName")
		{
			RowContent = SNew(STextBlock).Text(FText::FromName(Object->ObjectName)).ToolTipText(FText::FromName(Object->ObjectName)).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else if (ColumnName == "SerialSize")
		{
			RowContent = SNew(STextBlock).Text(FText::AsMemory(Object->SerialSize, EMemoryUnitStandard::IEC)).ToolTipText(FText::AsNumber(Object->SerialSize)).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "SerialOffset")
		{
			RowContent = SNew(STextBlock).Text(FText::AsNumber(Object->SerialOffset)).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "bIsAsset")
		{
			RowContent = SNew(STextBlock).Text(FText::FromString(Object->bIsAsset ? TEXT("true") : TEXT("false"))).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "bNotForClient")
		{
			RowContent = SNew(STextBlock).Text(FText::FromString(Object->bNotForClient ? TEXT("true") : TEXT("false"))).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "bNotForServer")
		{
			RowContent = SNew(STextBlock).Text(FText::FromString(Object->bNotForServer ? TEXT("true") : TEXT("false"))).Justification(ETextJustify::Center);
		}
		else if (ColumnName == "ClassIndex")
		{
			RowContent = SNew(STextBlock).Text(FText::FromName(Object->ClassName)).ToolTipText(FText::FromName(Object->ClassName)).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else if (ColumnName == "SuperIndex")
		{
			RowContent = SNew(STextBlock).Text(FText::FromName(Object->Super)).ToolTipText(FText::FromName(Object->Super)).Justification(ETextJustify::Right).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else if (ColumnName == "TemplateIndex")
		{
			RowContent = SNew(STextBlock).Text(FText::FromName(Object->TemplateObject)).ToolTipText(FText::FromName(Object->TemplateObject)).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else if (ColumnName == "FullPath")
		{
			RowContent = SNew(STextBlock).Text(FText::FromName(Object->ObjectPath)).ToolTipText(FText::FromName(Object->ObjectPath)).Margin(FMargin(LeftMargin, 0.f, 0.f, 0.f));
		}
		else if (ColumnName == "Dependencies")
		{
			RowContent = SNew(SComboBox<TSharedPtr<FName>>)
				.ContentPadding(FMargin(6.0f, 2.0f))
				.OptionsSource(&Dependencies)
				.OnGenerateWidget_Lambda([](TSharedPtr<FName> Item)
				{
					return SNew(STextBlock).Text(FText::FromName(*Item)).ToolTipText(FText::FromName(*Item)).Margin(FMargin(2.f, 2.f, 2.f, 2.f));
				})
				.Content()
				[
					SNew(STextBlock).Text(LOCTEXT("Dependencies", "Dependencies"))
				];
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
	TWeakPtr<FObjectExportEx> WeakObject;

	TArray<TSharedPtr<FName>> Dependencies;
};

SAssetSummaryView::SAssetSummaryView()
{

}

SAssetSummaryView::~SAssetSummaryView()
{
}

void SAssetSummaryView::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 4.f)
		[
			SNew(STextBlock).Text(LOCTEXT("Tree_View_Summary", "Asset Summary:"))
		]

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
#else
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Guid", "Guid:")).ValueText(this, &SAssetSummaryView::GetGuid)
		]
#endif

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_bUnversioned", "bUnversioned:")).ValueText(this, &SAssetSummaryView::GetIsUnversioned)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_FileVersionUE4", "FileVersionUE4:")).ValueText(this, &SAssetSummaryView::GetFileVersionUE4)
		]
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_FileVersionUE5", "FileVersionUE5:")).ValueText(this, &SAssetSummaryView::GetFileVersionUE5)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_FileVersionLicenseeUE", "FileVersionLicenseeUE:")).ValueText(this, &SAssetSummaryView::GetFileVersionLicenseeUE)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_TotalHeaderSize", "TotalHeaderSize:")).ValueText(this, &SAssetSummaryView::GetTotalHeaderSize)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_PackageFlags", "PackageFlags:")).ValueText(this, &SAssetSummaryView::GetPackageFlags)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_BulkDataStartOffset", "BulkDataStartOffset:")).ValueText(this, &SAssetSummaryView::GetBulkDataStartOffset)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_GatherableTextDataCount", "GatherableTextDataCount:")).ValueText(this, &SAssetSummaryView::GetGatherableTextDataCount)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_GatherableTextDataOffset", "GatherableTextDataOffset:")).ValueText(this, &SAssetSummaryView::GetGatherableTextDataOffset)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_DependsOffset", "DependsOffset:")).ValueText(this, &SAssetSummaryView::GetDependsOffset)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_SoftPackageReferencesCount", "SoftPackageReferencesCount:")).ValueText(this, &SAssetSummaryView::GetSoftPackageReferencesCount)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_SoftPackageReferencesOffset", "SoftPackageReferencesOffset:")).ValueText(this, &SAssetSummaryView::GetSoftPackageReferencesOffset)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_SearchableNamesOffset", "SearchableNamesOffset:")).ValueText(this, &SAssetSummaryView::GetSearchableNamesOffset)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_ThumbnailTableOffset", "ThumbnailTableOffset:")).ValueText(this, &SAssetSummaryView::GetThumbnailTableOffset)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_AssetRegistryDataOffset", "AssetRegistryDataOffset:")).ValueText(this, &SAssetSummaryView::GetAssetRegistryDataOffset)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_WorldTileInfoDataOffset", "WorldTileInfoDataOffset:")).ValueText(this, &SAssetSummaryView::GetWorldTileInfoDataOffset)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.HeaderContent()
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_ImportObjects", "ImportObjects:"))
				.ValueContent()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Count", "Count:")).ValueText(this, &SAssetSummaryView::GetImportCount)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Offset", "Offset:")).ValueText(this, &SAssetSummaryView::GetImportOffset)
					]
				]
			]
			.BodyContent()
			[
				SAssignNew(ImportObjectListView, SListView<FObjectImportPtrType>)
				.ItemHeight(25.f)
				.SelectionMode(ESelectionMode::Multi)
				.ListItemsSource(&ImportObjects)
				.OnGenerateRow(this, &SAssetSummaryView::OnGenerateImportObjectRow)
				.HeaderRow
				(
					SAssignNew(ImportObjectHeaderRow, SHeaderRow).Visibility(EVisibility::Visible)
				)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.HeaderContent()
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_ExportObjects", "ExportObjects:"))
				.ValueContent()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Count", "Count:")).ValueText(this, &SAssetSummaryView::GetExportCount)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Offset", "Offset:")).ValueText(this, &SAssetSummaryView::GetExportOffset)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Size", "Size:")).ValueText(this, &SAssetSummaryView::GetExportSize).ValueToolTipText(this, &SAssetSummaryView::GetExportSizeTooltip)
					]
				]
			]
			.BodyContent()
			[
				SAssignNew(ExportObjectListView, SListView<FObjectExportPtrType>)
				.ItemHeight(25.f)
				.SelectionMode(ESelectionMode::Multi)
				.ListItemsSource(&ExportObjects)
				.OnGenerateRow(this, &SAssetSummaryView::OnGenerateExportObjectRow)
				.HeaderRow
				(
					SAssignNew(ExportObjectHeaderRow, SHeaderRow).Visibility(EVisibility::Visible)
				)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.HeaderContent()
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Dependency", "Dependency packages:")).KeyToolTipText(LOCTEXT("Tree_View_Summary_DependencyTip", "Packages that this package depends on"))
				.ValueContent()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Count", "Count:")).ValueText(this, &SAssetSummaryView::GetDependencyCount)
					]
				]
			]
			.BodyContent()
			[
				SAssignNew(DependencyListView, SListView<FPackageInfoPtr>)
				.ItemHeight(25.f)
				.SelectionMode(ESelectionMode::Multi)
				.ListItemsSource(&DependencyList)
				.OnGenerateRow(this, &SAssetSummaryView::OnGenerateDependsRow)
				.HeaderRow
				(
					SNew(SHeaderRow).Visibility(EVisibility::Visible)

					+ SHeaderRow::Column(FName("Package"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Tree_View_Summary_Package", "Package"))
				)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.HeaderContent()
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Dependent", "Dependent packages:")).KeyToolTipText(LOCTEXT("Tree_View_Summary_DependentTip", "Packages that depends on this package. May be incorrect due to multi paks!"))
				.ValueContent()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Count", "Count:")).ValueText(this, &SAssetSummaryView::GetDependentCount)
					]
				]
			]
			.BodyContent()
			[
				SAssignNew(DependentListView, SListView<FPackageInfoPtr>)
				.ItemHeight(25.f)
				.SelectionMode(ESelectionMode::Multi)
				.ListItemsSource(&DependentList)
				.OnGenerateRow(this, &SAssetSummaryView::OnGenerateDependsRow)
				.HeaderRow
				(
					SNew(SHeaderRow).Visibility(EVisibility::Visible)

					+ SHeaderRow::Column(FName("Package"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Tree_View_Summary_Package", "Package"))
				)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 2.f)
		[
			SNew(SExpandableArea)
			.InitiallyCollapsed(true)
			.HeaderContent()
			[
				SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Names", "Names:"))
				.ValueContent()
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Count", "Count:")).ValueText(this, &SAssetSummaryView::GetNameCount)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SKeyValueRow).KeyText(LOCTEXT("Tree_View_Summary_Offset", "Offset:")).ValueText(this, &SAssetSummaryView::GetNameOffset)
					]
				]
			]
			.BodyContent()
			[
				SAssignNew(NamesListView, SListView<FNamePtrType>)
				.ItemHeight(25.f)
				.SelectionMode(ESelectionMode::Multi)
				.ListItemsSource(&PackageNames)
				.OnGenerateRow(this, &SAssetSummaryView::OnGenerateNameRow)
				.HeaderRow
				(
					SNew(SHeaderRow).Visibility(EVisibility::Visible)

					+ SHeaderRow::Column(FName("Name"))
					.FillWidth(1.f)
					.DefaultLabel(LOCTEXT("Tree_View_Summary_Name", "Name"))
				)
			]
		]
	];

	InsertColumn(ImportObjectHeaderRow, "Index");
	InsertColumn(ImportObjectHeaderRow, "ObjectName");
	InsertColumn(ImportObjectHeaderRow, "ClassName");
	InsertColumn(ImportObjectHeaderRow, "ClassPackage");
	InsertColumn(ImportObjectHeaderRow, "FullPath");

	InsertColumn(ExportObjectHeaderRow, "Index");
	InsertColumn(ExportObjectHeaderRow, "ObjectName");
	InsertColumn(ExportObjectHeaderRow, "ClassIndex", TEXT("ClassName"));
	InsertSortableColumn(ExportObjectHeaderRow, "SerialSize");
	InsertSortableColumn(ExportObjectHeaderRow, "SerialOffset");
	InsertColumn(ExportObjectHeaderRow, "FullPath");
	InsertColumn(ExportObjectHeaderRow, "bIsAsset");
	InsertColumn(ExportObjectHeaderRow, "bNotForClient");
	InsertColumn(ExportObjectHeaderRow, "bNotForServer");
	InsertColumn(ExportObjectHeaderRow, "TemplateIndex", TEXT("TemplateObject"));
	InsertColumn(ExportObjectHeaderRow, "SuperIndex", TEXT("Super"));
	InsertColumn(ExportObjectHeaderRow, "Dependencies");
}

void SAssetSummaryView::SetViewingPackage(FPakFileEntryPtr InPackage)
{
	ViewingPackage = InPackage;

	PackageNames = InPackage->AssetSummary->Names;
	ImportObjects = InPackage->AssetSummary->ObjectImports;
	ExportObjects = InPackage->AssetSummary->ObjectExports;
	//PreloadDependency = InPackage->AssetSummary->PreloadDependency;
	DependencyList = InPackage->AssetSummary->DependencyList;
	DependentList = InPackage->AssetSummary->DependentList;

	TotalExportSize = 0;
	for (FObjectExportPtrType ExportObject : ExportObjects)
	{
		if (ExportObject.IsValid())
		{
			TotalExportSize += ExportObject->SerialSize;
		}
	}

	OnSortExportObjects();

	NamesListView->RebuildList();
	ImportObjectListView->RebuildList();
	ExportObjectListView->RebuildList();
	//PreloadDependencyListView->RebuildList();
	DependencyListView->RebuildList();
	DependentListView->RebuildList();
}

TSharedRef<ITableRow> SAssetSummaryView::OnGenerateNameRow(FNamePtrType InName, const TSharedRef<class STableViewBase>& OwnerTable)
{
	return SNew(STableRow<FNamePtrType>, OwnerTable).Padding(FMargin(0.f, 2.f))
		[
			SNew(STextBlock).Text(FText::FromName(*InName))
		];
}

TSharedRef<ITableRow> SAssetSummaryView::OnGenerateImportObjectRow(FObjectImportPtrType InObject, const TSharedRef<class STableViewBase>& OwnerTable)
{
	return SNew(SImportObjectRow, InObject, OwnerTable);
}

TSharedRef<ITableRow> SAssetSummaryView::OnGenerateExportObjectRow(FObjectExportPtrType InObject, const TSharedRef<class STableViewBase>& OwnerTable)
{
	return SNew(SExportObjectRow, InObject, OwnerTable);
}

TSharedRef<ITableRow> SAssetSummaryView::OnGenerateDependsRow(FPackageInfoPtr InDepends, const TSharedRef<class STableViewBase>& OwnerTable)
{
	return SNew(STableRow<FPackageInfoPtr>, OwnerTable).Padding(FMargin(0.f, 2.f))
		[
			SNew(STextBlock).Text(FText::FromName(InDepends->PackageName)).ToolTipText(FText::FromName(InDepends->PackageName))
		];
}

void SAssetSummaryView::InsertColumn(TSharedPtr<SHeaderRow> InHeader, FName InId, const FString& InCloumnName)
{
	SHeaderRow::FColumn::FArguments ColumnArgs;
	ColumnArgs
		.ColumnId(InId)
		.DefaultLabel(InCloumnName.IsEmpty() ? FText::FromName(InId) : FText::FromString(InCloumnName))
		.HAlignHeader(HAlign_Fill)
		.VAlignHeader(VAlign_Fill)
		.HeaderContentPadding(FMargin(2.f))
		.HAlignCell(HAlign_Fill)
		.VAlignCell(VAlign_Fill);

	InHeader->AddColumn(ColumnArgs);
}

void SAssetSummaryView::InsertSortableColumn(TSharedPtr<SHeaderRow> InHeader, FName InId, const FString& InCloumnName)
{
	SHeaderRow::FColumn::FArguments ColumnArgs;
	ColumnArgs
		.ColumnId(InId)
		.DefaultLabel(InCloumnName.IsEmpty() ? FText::FromName(InId) : FText::FromString(InCloumnName))
		.HAlignHeader(HAlign_Fill)
		.VAlignHeader(VAlign_Fill)
		.HeaderContentPadding(FMargin(2.f))
		.HAlignCell(HAlign_Fill)
		.VAlignCell(VAlign_Fill)
		.SortMode(this, &SAssetSummaryView::GetSortModeForColumn, InId)
		.OnSort(this, &SAssetSummaryView::OnSortModeChanged);

	InHeader->AddColumn(ColumnArgs);
}

void SAssetSummaryView::OnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type SortMode)
{
	if (ColumnId != "SerialSize" && ColumnId != "SerialOffset")
	{
		return;
	}

	LastSortColumn = ColumnId;
	LastSortMode = SortMode;
	OnSortExportObjects();
	ExportObjectListView->RebuildList();
}

EColumnSortMode::Type SAssetSummaryView::GetSortModeForColumn(const FName ColumnId) const
{
	if (ColumnId != LastSortColumn)
	{
		return EColumnSortMode::None;
	}

	return LastSortMode;
}

void SAssetSummaryView::OnSortExportObjects()
{
	if (ExportObjects.Num() <= 0)
	{
		return;
	}

	if (LastSortColumn == "SerialSize")
	{
		ExportObjects.Sort([this](const FObjectExportPtrType& Lhs, const FObjectExportPtrType& Rhs) -> bool
		{
			if (LastSortMode == EColumnSortMode::Ascending)
			{
				return Lhs->SerialSize < Rhs->SerialSize;
			}
			else
			{
				return Lhs->SerialSize > Rhs->SerialSize;
			}
		});
	}
	else if (LastSortColumn == "SerialOffset")
	{
		ExportObjects.Sort([this](const FObjectExportPtrType& Lhs, const FObjectExportPtrType& Rhs) -> bool
		{
			if (LastSortMode == EColumnSortMode::Ascending)
			{
				return Lhs->SerialOffset < Rhs->SerialOffset;
			}
			else
			{
				return Lhs->SerialOffset > Rhs->SerialOffset;
			}
		});
	}
}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
#else
FORCEINLINE FText SAssetSummaryView::GetGuid() const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return ViewingPackage.IsValid() ? FText::FromString(ViewingPackage->AssetSummary->PackageSummary.Guid.ToString()) : FText();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif

FORCEINLINE FText SAssetSummaryView::GetIsUnversioned() const
{
	return ViewingPackage.IsValid() ? FText::FromString(ViewingPackage->AssetSummary->PackageSummary.bUnversioned ? TEXT("true") : TEXT("false")) : FText();
}

FORCEINLINE FText SAssetSummaryView::GetFileVersionUE4() const
{
	return ViewingPackage.IsValid() ? FText::AsNumber(ViewingPackage->AssetSummary->PackageSummary.GetFileVersionUE().FileVersionUE4) : FText();
}

FORCEINLINE FText SAssetSummaryView::GetFileVersionUE5() const
{
	return ViewingPackage.IsValid() ? FText::AsNumber(ViewingPackage->AssetSummary->PackageSummary.GetFileVersionUE().FileVersionUE5) : FText();
}

FORCEINLINE FText SAssetSummaryView::GetFileVersionLicenseeUE() const
{
	return ViewingPackage.IsValid() ? FText::AsNumber(ViewingPackage->AssetSummary->PackageSummary.GetFileVersionLicenseeUE()) : FText();
}

FORCEINLINE FText SAssetSummaryView::GetPackageFlags() const
{
	return ViewingPackage.IsValid() ? FText::FromString(FString::Printf(TEXT("0x%X"), ViewingPackage->AssetSummary->PackageSummary.GetPackageFlags())) : FText();
}

FORCEINLINE FText SAssetSummaryView::GetExportSize() const
{
	return FText::AsMemory(TotalExportSize, EMemoryUnitStandard::IEC);
}

FORCEINLINE FText SAssetSummaryView::GetExportSizeTooltip() const
{
	return FText::AsNumber(TotalExportSize);
}

FORCEINLINE FText SAssetSummaryView::GetDependencyCount() const
{
	return ViewingPackage.IsValid() ? FText::AsNumber(ViewingPackage->AssetSummary->DependencyList.Num()) : FText();
}

FORCEINLINE FText SAssetSummaryView::GetDependentCount() const
{
	return ViewingPackage.IsValid() ? FText::AsNumber(ViewingPackage->AssetSummary->DependentList.Num()) : FText();
}

DEFINE_GET_MEMBER_FUNCTION_NUMBER(TotalHeaderSize)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(NameCount)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(NameOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(ExportCount)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(ExportOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(ImportCount)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(ImportOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(GatherableTextDataCount)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(GatherableTextDataOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(DependsOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(SoftPackageReferencesCount)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(SoftPackageReferencesOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(SearchableNamesOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(ThumbnailTableOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(AssetRegistryDataOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(BulkDataStartOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(WorldTileInfoDataOffset)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(PreloadDependencyCount)
DEFINE_GET_MEMBER_FUNCTION_NUMBER(PreloadDependencyOffset)

#undef LOCTEXT_NAMESPACE
