// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SUnrealPakViewer.h"

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "IPlatformFilePak.h"
#include "Misc/CoreDelegates.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "UnrealPakViewer"

void SUnrealPakViewer::Construct(const FArguments& InArgs)
{
	FString EncryptionKeyString;
	FParse::Value(FCommandLine::Get(), TEXT("aes="), EncryptionKeyString, false);

	if (EncryptionKeyString.Len() > 0)
	{
		const uint32 RequiredKeyLength = sizeof(AESKey.Key);

		// Error checking
		if (EncryptionKeyString.Len() < RequiredKeyLength)
		{
			UE_LOG(LogUnrealPakViewer, Error, TEXT("AES encryption key must be %d characters long"), RequiredKeyLength);
		}

		if (EncryptionKeyString.Len() > RequiredKeyLength)
		{
			UE_LOG(LogUnrealPakViewer, Warning, TEXT("AES encryption key is more than %d characters long, so will be truncated!"), RequiredKeyLength);
			EncryptionKeyString = EncryptionKeyString.Left(RequiredKeyLength);
		}

		if (!FCString::IsPureAnsi(*EncryptionKeyString))
		{
			UE_LOG(LogUnrealPakViewer, Error, TEXT("AES encryption key must be a pure ANSI string!"));
		}

		ANSICHAR* AsAnsi = TCHAR_TO_ANSI(*EncryptionKeyString);
		check(TCString<ANSICHAR>::Strlen(AsAnsi) == RequiredKeyLength);
		FMemory::Memcpy(AESKey.Key, AsAnsi, RequiredKeyLength);
		UE_LOG(LogUnrealPakViewer, Display, TEXT("Parsed AES encryption key from command line."));

		if (AESKey.IsValid())
		{
			FCoreDelegates::GetPakEncryptionKeyDelegate().BindLambda([this](uint8 OutKey[32]) { FMemory::Memcpy(OutKey, AESKey.Key, sizeof(AESKey.Key)); });
		}
	}

	SAssignNew(PakFilePathTextBox, SEditableTextBox).IsReadOnly(true);
	SAssignNew(ExtractFolderPathTextBox, SEditableTextBox).IsReadOnly(true);

	ChildSlot
	[
		SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("PakFilePathText", "PakFilePath: "))
				]

				+SHorizontalBox::Slot()
				[
					PakFilePathTextBox->AsShared()
				]

				+SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("PakFileButtonText", "Open")).HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center).OnClicked(this, &SUnrealPakViewer::OnOpenPAKButtonClicked)
				]
			]

			// Separator
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 2)
			[
				SNew(SSeparator)
			]

			+SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				[
					SAssignNew(TreeViewPtr, STreeView<TSharedPtr<FTreeItem>>)
					.TreeItemsSource(&TreeRootItems)
					.OnGenerateRow(this, &SUnrealPakViewer::GenerateTreeRow)
					//.OnItemScrolledIntoView(this, &SPathView::TreeItemScrolledIntoView)
					.ItemHeight(18)
					.SelectionMode(ESelectionMode::Single)
					.OnSelectionChanged(this, &SUnrealPakViewer::TreeSelectionChanged)
					//.OnExpansionChanged(this, &SPathView::TreeExpansionChanged)
					.OnGetChildren(this, &SUnrealPakViewer::GetChildrenForTree)
					//.OnSetExpansionRecursive(this, &SPathView::SetTreeItemExpansionRecursive)
					//.OnContextMenuOpening(this, &SPathView::MakePathViewContextMenu)
					.OnContextMenuOpening(this, &SUnrealPakViewer::OnContextMenuOpening)
					.ClearSelectionOnClick(false)
				]

				// Separator
				+SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 2, 0)
				[
					SNew(SSeparator).Orientation(EOrientation::Orient_Vertical)
				]

				+SHorizontalBox::Slot()
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot().Padding(0, 5, 0, 0)
					[
						SNew(SVerticalBox)

						+SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("FileCountText", "File count: "))
							]

							+SHorizontalBox::Slot()
							[
								SAssignNew(FileCountTextBlock, STextBlock)
							]
						]

						+SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("OffsetText", "Offset: "))
							]

							+SHorizontalBox::Slot()
							[
								SAssignNew(OffsetTextBlock, STextBlock)
							]
						]

						+SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("SizeText", "Size: "))
							]

							+SHorizontalBox::Slot()
							[
								SAssignNew(SizeTextBlock, STextBlock)
							]
						]

						+SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("SerializedSizeText", "Extra serialized size: "))
							]

							+SHorizontalBox::Slot()
							[
								SAssignNew(SerializedSizeTextBlock, STextBlock)
							]
						]

						+SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("UncompressedSizeText", "Uncompressed size: "))
							]

							+SHorizontalBox::Slot()
							[
								SAssignNew(UncompressedSizeTextBlock, STextBlock)
							]
						]

						+SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("CompressionMethodText", "Compression method: "))
							]

							+SHorizontalBox::Slot()
							[
								SAssignNew(CompressionMethodTextBlock, STextBlock)
							]
						]

						+SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("CompressionBlockCountText", "Compression block count: "))
							]

							+SHorizontalBox::Slot()
							[
								SAssignNew(CompressionBlockCountTextBlock, STextBlock)
							]
						]

						+SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("CompressionBlockSizeText", "Compression block size: "))
							]

							+SHorizontalBox::Slot()
							[
								SAssignNew(CompressionBlockSizeTextBlock, STextBlock)
							]
						]

						+SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("SHA1Text", "SHA1: "))
							]

							+SHorizontalBox::Slot()
							[
								SAssignNew(SHA1TextBox, SEditableTextBox).IsReadOnly(true)
							]
						]

						+SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
						[
							SNew(SHorizontalBox)

							+SHorizontalBox::Slot().AutoWidth().HAlign(EHorizontalAlignment::HAlign_Center).VAlign(EVerticalAlignment::VAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("IsEncryptedText", "Is encrypted: "))
							]

							+SHorizontalBox::Slot()
							[
								SAssignNew(IsEncryptedTextBlock, STextBlock)
							]
						]
					]
				]
			]
		]
	];
}

FReply SUnrealPakViewer::OnOpenPAKButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		TArray<FString> OpenPakFilenames;
		int32 FilterIndex = -1;

		DesktopPlatform->OpenFileDialog(
			nullptr,
			LOCTEXT("OpenPakFileDialogTitle", "OpenPakFile").ToString(),
			TEXT(""),
			TEXT(""),
			TEXT("Pak Files (*.pak)|*.pak|All Files (*.*)|*.*"),
			EFileDialogFlags::None,
			OpenPakFilenames,
			FilterIndex);

		if (OpenPakFilenames.Num() > 0)
		{
			GenerateTreeItemsFromPAK(OpenPakFilenames[0]);
		}
	}

	return FReply::Handled();
}

TSharedRef<ITableRow> SUnrealPakViewer::GenerateTreeRow(TSharedPtr<FTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(TreeItem.IsValid());

	FLinearColor TextColor = TreeItem->Children.Num() > 0 ? FLinearColor::Green : FLinearColor::White;

	return
		SNew(STableRow<TSharedPtr<FTreeItem>>, OwnerTable)
		[
			SNew(STextBlock).Text(FText::FromString(TreeItem->DisplayName)).ColorAndOpacity(FSlateColor(TextColor))
		];
}

void SUnrealPakViewer::GetChildrenForTree(TSharedPtr<FTreeItem> TreeItem, TArray<TSharedPtr<FTreeItem>>& OutChildren)
{
	TreeItem->SortChildrenIfNeeded();
	OutChildren = TreeItem->Children;
}

static FString FormatSize(int64 InSize)
{
	if (InSize < 1024)
	{
		return FString::Printf(TEXT("%lld B"), InSize);
	}
	else
	{
		const float KB = InSize / 1024.f;
		if (KB < 1024)
		{
			return FString::Printf(TEXT("%.3f K (%lld)"), KB, InSize);
		}
		else
		{
			const float MB = KB / 1024.f;
			return FString::Printf(TEXT("%.3f M (%lld)"), MB, InSize);
		}
	}
}

void SUnrealPakViewer::TreeSelectionChanged(TSharedPtr<FTreeItem> TreeItem, ESelectInfo::Type SelectInfo)
{
	if (!TreeItem.IsValid())
	{
		return;
	}

	FileCountTextBlock->SetText(FText::FromString(FString::Printf(TEXT("%d"), TreeItem->GetChildFileCounts())));
	SizeTextBlock->SetText(FText::FromString(FormatSize(TreeItem->GetTotalSize())));
	UncompressedSizeTextBlock->SetText(FText::FromString(FormatSize(TreeItem->GetTotalUncompressedSize())));
	IsEncryptedTextBlock->SetText(FText::FromString(TreeItem->IsEncrypted));
	OffsetTextBlock->SetText(FText::FromString(FString::Printf(TEXT("%lld"), TreeItem->Offset)));
	SHA1TextBox->SetText(FText::FromString(TreeItem->Hash));
	CompressionMethodTextBlock->SetText(FText::FromString(TreeItem->CompressionMethod));
	CompressionBlockCountTextBlock->SetText(FText::FromString(FString::Printf(TEXT("%d"), TreeItem->CompressionBlockCount)));
	CompressionBlockSizeTextBlock->SetText(FText::FromString(FString::Printf(TEXT("%u"), TreeItem->CompressionBlockSize)));
	SerializedSizeTextBlock->SetText(FText::FromString(FormatSize(TreeItem->GetTotalSerializedSize())));
}

static FString FormatCompressionMethod(ECompressionFlags InFlag)
{
	switch (InFlag)
	{
	case COMPRESS_None: return TEXT("None");
	case COMPRESS_ZLIB: return TEXT("ZLIB");
	case COMPRESS_GZIP: return TEXT("GZIP");
	case COMPRESS_Custom: return TEXT("Custom");
	case COMPRESS_BiasMemory: return TEXT("BiasMemory");
	case COMPRESS_BiasSpeed: return TEXT("BiasSpeed");
	case COMPRESS_OverridePlatform: return TEXT("OverridePlatform");
	default: return TEXT("Unknown");
	}
}

void SUnrealPakViewer::GenerateTreeItemsFromPAK(const FString& PAKFile)
{
	PakFilePathTextBox->SetText(FText::FromString(TEXT("")));
	TreeRootItems.Empty();
	TreeViewPtr->RebuildList();

	PakFile = MakeShareable(new FPakFile(*PAKFile, false));
	if (!PakFile->IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PakFileNotValidMessage", "Pak file is not valid!"));
		return;
	}

	UE_LOG(LogUnrealPakViewer, Display, TEXT("Pak file[%s], PakInfo size[%lld]."), *PAKFile, PakFile->GetInfo().IndexSize + PakFile->GetInfo().GetSerializedSize());

	TSharedPtr<FTreeItem> VirtualRootItem = MakeShareable(new FTreeItem());
	VirtualRootItem->DisplayName = FPaths::GetCleanFilename(PAKFile);
	TreeRootItems.Add(VirtualRootItem);

	PakFilePathTextBox->SetText(FText::FromString(FPaths::ConvertRelativePathToFull(PAKFile)));
	TArray<FPakFile::FFileIterator> Records;
	for (FPakFile::FFileIterator It(*PakFile); It; ++It)
	{
		Records.Add(It);
	}

	struct FOffsetSort
	{
		FORCEINLINE bool operator()(const FPakFile::FFileIterator& A, const FPakFile::FFileIterator& B) const
		{
			return A.Info().Offset < B.Info().Offset;
		}
	};

	Records.Sort(FOffsetSort());

	for (auto It : Records)
	{
		const FPakEntry& Entry = It.Info();

		TSharedPtr<FTreeItem> Item = MakeTreeItemsFromPath(VirtualRootItem, It.Filename());
		if (Item.IsValid())
		{
			Item->Size = Entry.Size;
			Item->UncompressedSize = Entry.UncompressedSize;
			Item->Offset = Entry.Offset;
			Item->IsEncrypted = Entry.bEncrypted ? TEXT("true") : TEXT("false");
			Item->Hash = BytesToHex(Entry.Hash, sizeof(Entry.Hash));
			Item->CompressionMethod = FormatCompressionMethod((ECompressionFlags)Entry.CompressionMethod);
			Item->CompressionBlockCount = Entry.CompressionBlocks.Num();
			Item->CompressionBlockSize = Entry.CompressionBlockSize;
			Item->SerializedSize = Entry.GetSerializedSize(PakFile->GetInfo().Version);
		}
	}

	TreeViewPtr->RebuildList();
}

TSharedPtr<FTreeItem> SUnrealPakViewer::MakeTreeItemsFromPath(TSharedPtr<FTreeItem> Parent, const FString& Path)
{
	TArray<FString> StringItems;

	FString Paths = Path;
	FString Left, Right;
	while (Paths.Split(TEXT("/"), &Left, &Right))
	{
		StringItems.Emplace(Left);
		Paths = Right;
	}

	if (!Paths.IsEmpty())
	{
		StringItems.Emplace(Paths);
	}

	if (StringItems.Num() <= 0)
	{
		return nullptr;
	}

	int32 StartIndex = 0;
	TSharedPtr<FTreeItem> RootItem = Parent;
	if (!RootItem.IsValid())
	{
		RootItem = MakeShareable(new FTreeItem());
		RootItem->DisplayName = StringItems[0];
		TreeRootItems.Add(RootItem);
		StartIndex = 1;
	}

	for (int32 i = StartIndex; i < StringItems.Num(); ++i)
	{
		TSharedPtr<FTreeItem> ChildItem = RootItem->GetChild(StringItems[i]);
		if (!ChildItem.IsValid())
		{
			ChildItem = MakeShareable(new FTreeItem());
			ChildItem->DisplayName = StringItems[i];
			ChildItem->Parent = RootItem;
			RootItem->Children.Add(ChildItem);
		}

		RootItem = ChildItem;
	}

	return RootItem;
}

TSharedPtr<SWidget> SUnrealPakViewer::OnContextMenuOpening()
{
	if (TreeViewPtr->GetSelectedItems().Num() <= 0)
	{
		return nullptr;
	}

	TSharedPtr<FExtender> Extender;
	FMenuBuilder MenuBuilder(true, nullptr, Extender);

	MenuBuilder.AddMenuEntry(LOCTEXT("ExtractText", "Extract..."),
		LOCTEXT("ExtractTooltip", "Extract this file or folder to disk."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SUnrealPakViewer::ExecuteExtract)));

	return MenuBuilder.MakeWidget();
}

void SUnrealPakViewer::ExecuteExtract()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	FString SelectedFolder;

	const bool bOpenResult = DesktopPlatform->OpenDirectoryDialog(
		nullptr,
		LOCTEXT("OpenExtractDialogTitleText", "Select extract path").ToString(),
		TEXT(""),
		SelectedFolder);

	if (!bOpenResult || SelectedFolder.IsEmpty())
	{
		return;
	}

	TSharedPtr<FTreeItem> SelectedItem = TreeViewPtr->GetSelectedItems()[0];
	TArray<FString> Files;
	SelectedItem->GetAllFiles(Files);

	FPakEntry OutEntry;

	for (const FString& File : Files)
	{
		const FString TrueFilePath = File.Mid(File.Find(TEXT("/")) + 1);
		if (!PakFile->Find(FString::Printf(TEXT("%s%s"), *PakFile->GetMountPoint(), *TrueFilePath), &OutEntry))
		{
			continue;
		}

		ExtractFileFromPak(OutEntry, File, SelectedFolder);
	}
}

bool SUnrealPakViewer::UncompressCopyFile(FArchive& Dest, FArchive& Source, const FPakEntry& Entry, uint8*& PersistentBuffer, int64& BufferSize, const FAES::FAESKey& Key, const FPakFile& PakFile)
{
	if (Entry.UncompressedSize == 0)
	{
		return false;
	}

	// The compression block size depends on the bit window that the PAK file was originally created with. Since this isn't stored in the PAK file itself,
	// we can use FCompression::CompressMemoryBound as a guideline for the max expected size to avoid unncessary reallocations, but we need to make sure
	// that we check if the actual size is not actually greater (eg. UE-59278).
	int32 MaxCompressionBlockSize = FCompression::CompressMemoryBound((ECompressionFlags)Entry.CompressionMethod, Entry.CompressionBlockSize);
	for (const FPakCompressedBlock& Block : Entry.CompressionBlocks)
	{
		MaxCompressionBlockSize = FMath::Max<int32>(MaxCompressionBlockSize, Block.CompressedEnd - Block.CompressedStart);
	}

	int64 WorkingSize = Entry.CompressionBlockSize + MaxCompressionBlockSize;
	if (BufferSize < WorkingSize)
	{
		PersistentBuffer = (uint8*)FMemory::Realloc(PersistentBuffer, WorkingSize);
		BufferSize = WorkingSize;
	}

	uint8* UncompressedBuffer = PersistentBuffer + MaxCompressionBlockSize;

	for (uint32 BlockIndex = 0, BlockIndexNum = Entry.CompressionBlocks.Num(); BlockIndex < BlockIndexNum; ++BlockIndex)
	{
		uint32 CompressedBlockSize = Entry.CompressionBlocks[BlockIndex].CompressedEnd - Entry.CompressionBlocks[BlockIndex].CompressedStart;
		uint32 UncompressedBlockSize = (uint32)FMath::Min<int64>(Entry.UncompressedSize - Entry.CompressionBlockSize*BlockIndex, Entry.CompressionBlockSize);
		Source.Seek(Entry.CompressionBlocks[BlockIndex].CompressedStart + (PakFile.GetInfo().HasRelativeCompressedChunkOffsets() ? Entry.Offset : 0));
		uint32 SizeToRead = Entry.bEncrypted ? Align(CompressedBlockSize, FAES::AESBlockSize) : CompressedBlockSize;
		Source.Serialize(PersistentBuffer, SizeToRead);

		if (Entry.bEncrypted)
		{
			FAES::DecryptData(PersistentBuffer, SizeToRead, Key);
		}

		if (!FCompression::UncompressMemory((ECompressionFlags)Entry.CompressionMethod, UncompressedBuffer, UncompressedBlockSize, PersistentBuffer, CompressedBlockSize))
		{
			return false;
		}
		Dest.Serialize(UncompressedBuffer, UncompressedBlockSize);
	}

	return true;
}

bool SUnrealPakViewer::BufferedCopyFile(FArchive& Dest, FArchive& Source, const FPakEntry& Entry, void* Buffer, int64 BufferSize, const FAES::FAESKey& Key)
{
	// Align down
	BufferSize = BufferSize & ~(FAES::AESBlockSize - 1);
	int64 RemainingSizeToCopy = Entry.Size;
	while (RemainingSizeToCopy > 0)
	{
		const int64 SizeToCopy = FMath::Min(BufferSize, RemainingSizeToCopy);
		// If file is encrypted so we need to account for padding
		int64 SizeToRead = Entry.bEncrypted ? Align(SizeToCopy, FAES::AESBlockSize) : SizeToCopy;

		Source.Serialize(Buffer, SizeToRead);
		if (Entry.bEncrypted)
		{
			FAES::DecryptData((uint8*)Buffer, SizeToRead, Key);
		}
		Dest.Serialize(Buffer, SizeToCopy);
		RemainingSizeToCopy -= SizeToRead;
	}
	return true;
}

bool SUnrealPakViewer::ExtractFileFromPak(const FPakEntry& Entry, const FString& FileInPak, const FString& DestFolder)
{
	if (!PakFile.IsValid() || !PakFile->IsValid())
	{
		return false;
	}

	FString DestPath(DestFolder);
	FArchive& PakReader = *PakFile->GetSharedReader(NULL);
	const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
	void* Buffer = FMemory::Malloc(BufferSize);
	int64 CompressionBufferSize = 0;
	uint8* PersistantCompressionBuffer = NULL;
	bool bResult = false;

	PakReader.Seek(Entry.Offset);
	FPakEntry EntryInfo;
	EntryInfo.Serialize(PakReader, PakFile->GetInfo().Version);
	if (EntryInfo == Entry)
	{
		FString DestFilename(DestPath / FileInPak);

		TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*DestFilename));
		if (FileHandle)
		{
			if (Entry.CompressionMethod == COMPRESS_None)
			{
				BufferedCopyFile(*FileHandle, PakReader, Entry, Buffer, BufferSize, AESKey);
			}
			else
			{
				UncompressCopyFile(*FileHandle, PakReader, Entry, PersistantCompressionBuffer, CompressionBufferSize, AESKey, *PakFile);
			}

			bResult = true;
		}
		else
		{
			UE_LOG(LogUnrealPakViewer, Error, TEXT("Unable to create file \"%s\"."), *DestFilename);
		}
	}
	else
	{
		UE_LOG(LogUnrealPakViewer, Error, TEXT("Serialized hash mismatch for \"%s\"."), *FileInPak);
	}

	FMemory::Free(Buffer);
	FMemory::Free(PersistantCompressionBuffer);

	return bResult;
}

#undef LOCTEXT_NAMESPACE
