// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealPakViewer.h"

#include "Misc/AES.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

/** The struct representing an item in the asset tree */
struct FTreeItem : public TSharedFromThis<FTreeItem>
{
	FString DisplayName;

	int64 Offset;
	int64 Size;
	int64 UncompressedSize;
	FString CompressionMethod;
	FString Hash;
	FString IsEncrypted;
	int32 CompressionBlockCount;
	uint32 CompressionBlockSize;
	int64 SerializedSize;

	/** The children of this tree item */
	TArray<TSharedPtr<FTreeItem>> Children;

	/** The parent folder for this item */
	TWeakPtr<FTreeItem> Parent;

	TSharedPtr<FTreeItem> FindItemRecursive(const FString& InDisplayName)
	{
		if (DisplayName.Equals(InDisplayName))
		{
			return SharedThis(this);
		}

		for (const auto& Child : Children)
		{
			const TSharedPtr<FTreeItem>& Item = Child->FindItemRecursive(InDisplayName);
			if (Item.IsValid())
			{
				return Item;
			}
		}

		return nullptr;
	}

	TSharedPtr<FTreeItem> GetChild(const FString& InDisplayName) const
	{
		for (const auto& Child : Children)
		{
			if (Child->DisplayName.Equals(InDisplayName))
			{
				return Child;
			}
		}

		return nullptr;
	}

	void SortChildrenIfNeeded()
	{
		if (bChildrenRequireSort)
		{
			Children.Sort([](TSharedPtr<FTreeItem> A, TSharedPtr<FTreeItem> B) -> bool
			{
				if ((A->Children.Num() <= 0 && B->Children.Num() <= 0) ||
					(A->Children.Num() > 0 && B->Children.Num() > 0))
				{
					return A->DisplayName < B->DisplayName;
				}

				return A->Children.Num() > B->Children.Num();
			});

			bChildrenRequireSort = false;
		}
	}

	int32 GetChildFileCounts()
	{
		if (ChildFileCounts <= 0)
		{
			if (Children.Num() > 0)
			{
				for (const auto& Child : Children)
				{
					if (Child->Children.Num() > 0)
					{
						ChildFileCounts += Child->GetChildFileCounts();
					}
					else
					{
						ChildFileCounts += 1;
					}
				}
			}
			else
			{
				ChildFileCounts = 1;
			}
		}

		return ChildFileCounts;
	}

	int32 GetTotalSize()
	{
		if (TotalSize <= 0)
		{
			if (Children.Num() > 0)
			{
				for (const auto& Child : Children)
				{
					if (Child->Children.Num() > 0)
					{
						TotalSize += Child->GetTotalSize();
					}
					else
					{
						TotalSize += Child->Size;
					}
				}
			}
			else
			{
				TotalSize = Size;
			}
		}

		return TotalSize;
	}

	int32 GetTotalUncompressedSize()
	{
		if (TotalUncompressedSize <= 0)
		{
			if (Children.Num() > 0)
			{
				for (const auto& Child : Children)
				{
					if (Child->Children.Num() > 0)
					{
						TotalUncompressedSize += Child->GetTotalUncompressedSize();
					}
					else
					{
						TotalUncompressedSize += Child->UncompressedSize;
					}
				}
			}
			else
			{
				TotalUncompressedSize = UncompressedSize;
			}
		}

		return TotalUncompressedSize;
	}

	int32 GetTotalSerializedSize()
	{
		if (TotalSerializedSize <= 0)
		{
			if (Children.Num() > 0)
			{
				for (const auto& Child : Children)
				{
					if (Child->Children.Num() > 0)
					{
						TotalSerializedSize += Child->GetTotalSerializedSize();
					}
					else
					{
						TotalSerializedSize += Child->SerializedSize;
					}
				}
			}
			else
			{
				TotalSerializedSize = SerializedSize;
			}
		}

		return TotalSerializedSize;
	}

	FString GetPath()
	{
		if (Parent.IsValid())
		{
			return FString::Printf(TEXT("%s/%s"), *Parent.Pin()->GetPath(), *DisplayName);
		}
		else
		{
			return DisplayName;
		}
	}

	void GetAllFiles(TArray<FString>& Files)
	{
		if (Children.Num() <= 0)
		{
			Files.Add(GetPath());
		}
		else
		{
			for (const auto& Child : Children)
			{
				Child->GetAllFiles(Files);
			}
		}
	}

private:
	bool bChildrenRequireSort = true;
	int32 ChildFileCounts = 0;
	int64 TotalSize = 0;
	int64 TotalUncompressedSize = 0;
	int64 TotalSerializedSize = 0;
};

/**
 *
 */
class SUnrealPakViewer : public SCompoundWidget
{
public:
	/**
	 * Slate arguments
	 */
	SLATE_BEGIN_ARGS(SUnrealPakViewer)
	{
	}

	SLATE_END_ARGS()

	/**
	 * Construct this Slate ui
	 * @param InArgs Slate arguments, not used
	 * @param Client Crash report client implementation object
	 */
	void Construct(const FArguments& InArgs);

protected:
	FReply OnOpenPAKButtonClicked();
	FReply OnAESKeyConfirmButtonClicked();

	void ShowAESKeyWindow();

	/** Creates a list item for the tree view */
	virtual TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<FTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Handler for returning a list of children associated with a particular tree node */
	void GetChildrenForTree(TSharedPtr<FTreeItem> TreeItem, TArray<TSharedPtr<FTreeItem>>& OutChildren);

	/** Handler for tree view selection changes */
	void TreeSelectionChanged(TSharedPtr<FTreeItem> TreeItem, ESelectInfo::Type SelectInfo);

	void GenerateTreeItemsFromPAK(const FString& PAKFile);

	TSharedPtr<FTreeItem> MakeTreeItemsFromPath(TSharedPtr<FTreeItem> Parent, const FString& Path);

	TSharedPtr<SWidget> OnContextMenuOpening();
	void ExecuteExtract();

	bool ExtractFileFromPak(const struct FPakEntry& PakEntry, const FString& FileInPak, const FString& DestFolder);
	bool UncompressCopyFile(class FArchive& Dest, class FArchive& Source, const struct FPakEntry& Entry, uint8*& PersistentBuffer, int64& BufferSize, const FAES::FAESKey& Key, const class FPakFile& PakFile);
	bool BufferedCopyFile(class FArchive& Dest, class FArchive& Source, const struct FPakEntry& Entry, void* Buffer, int64 BufferSize, const FAES::FAESKey& Key);

protected:
	TSharedPtr<class SEditableTextBox> PakFilePathTextBox;
	TSharedPtr<class SEditableTextBox> ExtractFolderPathTextBox;
	TSharedPtr<STreeView<TSharedPtr<FTreeItem>>> TreeViewPtr;

	// detail info
	TSharedPtr<class STextBlock> FileCountTextBlock;
	TSharedPtr<class STextBlock> OffsetTextBlock;
	TSharedPtr<class STextBlock> SizeTextBlock;
	TSharedPtr<class STextBlock> SerializedSizeTextBlock;
	TSharedPtr<class STextBlock> UncompressedSizeTextBlock;
	TSharedPtr<class STextBlock> CompressionMethodTextBlock;
	TSharedPtr<class STextBlock> CompressionBlockCountTextBlock;
	TSharedPtr<class STextBlock> CompressionBlockSizeTextBlock;
	TSharedPtr<class SEditableTextBox> SHA1TextBox;
	TSharedPtr<class STextBlock> IsEncryptedTextBlock;

	TSharedPtr<class SWindow> AESKeyWindow;
	TSharedPtr<class SEditableTextBox> AESKeyTextBox;

	/** The list of folders in the tree */
	TArray<TSharedPtr<FTreeItem>> TreeRootItems;

	TSharedPtr<class FPakFile> PakFile;

	FAES::FAESKey AESKey;
};
