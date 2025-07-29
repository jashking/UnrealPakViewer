#include "IoStoreAnalyzer.h"

#if ENABLE_IO_STORE_ANALYZER

#include "Algo/Sort.h"
#include "Async/AsyncFileHandle.h"
#include "Async/ParallelFor.h"
#include "Containers/ArrayView.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/UnrealMemory.h"
#include "Misc/Base64.h"
#include "Misc/Compression.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/AsyncLoading2.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/MemoryReader.h"
#include "UObject/NameBatchSerialization.h"
#include "UObject/ObjectVersion.h"
#include "IO/PackageStore.h"
#include "CommonDefines.h"

FIoStoreAnalyzer::FIoStoreAnalyzer()
{

}

FIoStoreAnalyzer::~FIoStoreAnalyzer()
{
	StopExtract();
	Reset();
}

bool FIoStoreAnalyzer::LoadPakFiles(const TArray<FString>& InPakPaths, const TArray<FString>& InDefaultAESKeys, int32 ContainerStartIndex)
{
	TArray<FString> UcasFiles;
	TArray<FString> UsedDefaultAESKeys;
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	for (int32 i = 0; i < InPakPaths.Num(); ++i)
	{
		const FString& PakPath = InPakPaths[i];
		if (PlatformFile.FileExists(*PakPath) && PakPath.EndsWith(".ucas"))
		{
			UcasFiles.Add(PakPath);
			UsedDefaultAESKeys.Add(InDefaultAESKeys.IsValidIndex(i) ? InDefaultAESKeys[i] : TEXT(""));
		}
	}

	if (UcasFiles.Num() <= 0)
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load iostore file failed! No ucas files!"));
		return false;
	}

	UE_LOG(LogPakAnalyzer, Log, TEXT("Start load iostore file count: %d."), UcasFiles.Num());

	Reset();
	DefaultAESKeys = UsedDefaultAESKeys;

	if (!InitializeGlobalReader(UcasFiles[0]))
	{
		UE_LOG(LogPakAnalyzer, Warning, TEXT("Load iostore global container failed!"));
	}

	if (!InitializeReaders(UcasFiles, DefaultAESKeys))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Read iostore files failed!"));
	}

	if (StoreContainers.Num() <= 0)
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Read iostore files failed! Create containers failed!"));
	}

	// Make tree roots
	PakTreeRoots.AddZeroed(StoreContainers.Num());
	PakFileSummaries.AddZeroed(StoreContainers.Num());
	for (int32 i = 0; i < StoreContainers.Num(); ++i)
	{
		PakTreeRoots[i] = MakeShared<FPakTreeEntry>(*FPaths::GetCleanFilename(StoreContainers[i].Summary.PakFilePath), StoreContainers[i].Summary.MountPoint, true);
		PakFileSummaries[i] = MakeShared<FPakFileSumary>();
		*PakFileSummaries[i] = StoreContainers[i].Summary;
	}

	{
		FScopeLock Lock(&CriticalSection);

		for (int32 i = 0; i < PackageInfos.Num(); ++i)
		{
			const FStorePackageInfo& Package = PackageInfos[i];
			if (!Package.PackageId.IsValid())
			{
				continue;
			}

			FPakEntry Entry;
			Entry.Offset = Package.ChunkInfo.Offset;
			Entry.UncompressedSize = Package.ChunkInfo.Size;
			Entry.Size = Package.SerializeSize;
			Entry.CompressionBlockSize = Package.CompressionBlockSize;
			Entry.CompressionBlocks.AddZeroed(Package.CompressionBlockCount);
			HexToBytes(Package.ChunkHash, Entry.Hash);
			Entry.SetEncrypted(StoreContainers[Package.ContainerIndex].bEncrypted);

			const FString FullPath = Package.PackageName.ToString() + TEXT(".") + Package.Extension.ToString();
			FPakTreeEntryPtr ResultEntry = InsertFileToTree(PakTreeRoots[Package.ContainerIndex], StoreContainers[Package.ContainerIndex].Summary, FullPath, Entry);
			if (ResultEntry.IsValid())
			{
				ResultEntry->OwnerPakIndex = Package.ContainerIndex + ContainerStartIndex;
				ResultEntry->CompressionMethod = Package.CompressionMethod;

				if (Package.AssetSummary.IsValid())
				{
					ResultEntry->AssetSummary = Package.AssetSummary;
				}

				PakFileSummaries[Package.ContainerIndex]->FileCount += 1;
			}

			FileToPackageIndex.Add(FullPath, i);
			if (!Package.DefaultClassName.IsNone())
			{
				DefaultClassMap.Add(Package.PackageName, Package.DefaultClassName);
			}
		}
	}

	for (const FPakTreeEntryPtr TreeRoot : PakTreeRoots)
	{
		RefreshTreeNode(TreeRoot);
		RefreshTreeNodeSizePercent(TreeRoot, TreeRoot);
		RefreshClassMap(TreeRoot, TreeRoot);
	}

	UE_LOG(LogPakAnalyzer, Log, TEXT("Finish load iostore file count: %d."), UcasFiles.Num());

	//FPakAnalyzerDelegates::OnPakLoadFinish.Broadcast();

	return true;
}

void FIoStoreAnalyzer::ExtractFiles(const FString& InOutputPath, TArray<FPakFileEntryPtr>& InFiles)
{
	StopExtract();

	FPakAnalyzerDelegates::OnExtractStart.ExecuteIfBound();

	TArray<int32> Packages;
	for (FPakFileEntryPtr File : InFiles)
	{
		const int32* Index = FileToPackageIndex.Find(TEXT("/") / File->Path);
		if (Index)
		{
			PendingExtracePackages.AddUnique(*Index);
		}
	}

	if (PendingExtracePackages.Num() <= 0)
	{
		return;
	}

	ExtractOutputPath = InOutputPath;
	ExtractThread.Add(Async(EAsyncExecution::Thread, [this]() { OnExtractFiles(); }));
}

void FIoStoreAnalyzer::CancelExtract()
{
	StopExtract();
}

void FIoStoreAnalyzer::SetExtractThreadCount(int32 InThreadCount)
{

}

void FIoStoreAnalyzer::Reset()
{
	FBaseAnalyzer::Reset();

	GlobalIoStoreReader.Reset();
	GlobalNameMap.Empty();

	ScriptObjectByGlobalIdMap.Empty();
	TocResources.Empty();
	ExportByGlobalIdMap.Empty();

	//for (FContainerInfo& Container : StoreContainers)
	//{
	//	Container.Reader.Reset();
	//}

	//StoreContainers.Empty();
	PackageInfos.Empty();
	FileToPackageIndex.Empty();

	DefaultAESKeys.Empty();
}

TSharedPtr<FIoStoreReader> FIoStoreAnalyzer::CreateIoStoreReader(const FString& InPath, const FString& InDefaultAESKey, FString& OutDecryptKey)
{
	TMap<FGuid, FAES::FAESKey> DecryptionKeys;
	if (!PreLoadIoStore(FPaths::SetExtension(InPath, TEXT("utoc")), FPaths::SetExtension(InPath, TEXT("ucas")), InDefaultAESKey, DecryptionKeys, OutDecryptKey))
	{
		return nullptr;
	}

	TSharedPtr<FIoStoreReader> Reader = MakeShared<FIoStoreReader>();
	FIoStatus Status = Reader->Initialize(*FPaths::ChangeExtension(InPath, TEXT("")), DecryptionKeys);
	if (Status.IsOk())
	{
		return Reader;
	}
	else
	{
		UE_LOG(LogPakAnalyzer, Warning, TEXT("Failed creating IoStore reader '%s' [%s]"), *InPath, *Status.ToString());
		return nullptr;
	}
}

bool FIoStoreAnalyzer::InitializeGlobalReader(const FString& InPakPath)
{
	FString DecryptKey;
	const FString GlobalContainerPath = FPaths::GetPath(InPakPath) / TEXT("global");
	GlobalIoStoreReader = CreateIoStoreReader(GlobalContainerPath, TEXT(""), DecryptKey);
	if (!GlobalIoStoreReader.IsValid())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("IoStore failed load global container: %s!"), *GlobalContainerPath);
		return false;
	}

	UE_LOG(LogPakAnalyzer, Display, TEXT("Loading script imports..."));

	TIoStatusOr<FIoBuffer> ScriptObjectsBuffer = GlobalIoStoreReader->Read(CreateIoChunkId(0, 0, EIoChunkType::ScriptObjects), FIoReadOptions());
	if (!ScriptObjectsBuffer.IsOk())
	{
		UE_LOG(LogPakAnalyzer, Warning, TEXT("IoStore failed reading initial load meta chunk from global container '%s'"), *GlobalContainerPath);
		return false;
	}

	FLargeMemoryReader ScriptObjectsArchive(ScriptObjectsBuffer.ValueOrDie().Data(), ScriptObjectsBuffer.ValueOrDie().DataSize());
	GlobalNameMap = LoadNameBatch(ScriptObjectsArchive);
	int32 NumScriptObjects = 0;
	ScriptObjectsArchive << NumScriptObjects;
	const FScriptObjectEntry* ScriptObjectEntries = reinterpret_cast<const FScriptObjectEntry*>(ScriptObjectsBuffer.ValueOrDie().Data() + ScriptObjectsArchive.Tell());
	for (int32 ScriptObjectIndex = 0; ScriptObjectIndex < NumScriptObjects; ++ScriptObjectIndex)
	{
		const FScriptObjectEntry& ScriptObjectEntry = ScriptObjectEntries[ScriptObjectIndex];
		FMappedName MappedName = ScriptObjectEntry.Mapped;
		check(MappedName.IsGlobal());
		FScriptObjectDesc& ScriptObjectDesc = ScriptObjectByGlobalIdMap.Add(ScriptObjectEntry.GlobalIndex);
		ScriptObjectDesc.Name = GlobalNameMap[MappedName.GetIndex()].ToName(MappedName.GetNumber());
		ScriptObjectDesc.GlobalImportIndex = ScriptObjectEntry.GlobalIndex;
		ScriptObjectDesc.OuterIndex = ScriptObjectEntry.OuterIndex;
	}
	for (auto& KV : ScriptObjectByGlobalIdMap)
	{
		FScriptObjectDesc& ScriptObjectDesc = KV.Get<1>();
		if (ScriptObjectDesc.FullName.IsNone())
		{
			TArray<FScriptObjectDesc*> ScriptObjectStack;
			FScriptObjectDesc* Current = &ScriptObjectDesc;
			FString FullName;
			while (Current)
			{
				if (!Current->FullName.IsNone())
				{
					FullName = Current->FullName.ToString();
					break;
				}
				ScriptObjectStack.Push(Current);
				Current = ScriptObjectByGlobalIdMap.Find(Current->OuterIndex);
			}
			while (ScriptObjectStack.Num() > 0)
			{
				Current = ScriptObjectStack.Pop();
				FullName /= Current->Name.ToString();
				Current->FullName = FName(FullName);
			}
		}
	}

	//UE_LOG(LogPakAnalyzer, Display, TEXT("IoStore loading global name map..."));

	//TIoStatusOr<FIoBuffer> GlobalNamesIoBuffer = GlobalIoStoreReader->Read(CreateIoChunkId(0, 0, EIoChunkType::LoaderGlobalNames), FIoReadOptions());
	//if (!GlobalNamesIoBuffer.IsOk())
	//{
	//	UE_LOG(LogPakAnalyzer, Warning, TEXT("IoStore failed reading names chunk from global container '%s'"), *GlobalContainerPath);
	//	return false;
	//}

	//TIoStatusOr<FIoBuffer> GlobalNameHashesIoBuffer = GlobalIoStoreReader->Read(CreateIoChunkId(0, 0, EIoChunkType::LoaderGlobalNameHashes), FIoReadOptions());
	//if (!GlobalNameHashesIoBuffer.IsOk())
	//{
	//	UE_LOG(LogPakAnalyzer, Warning, TEXT("IoStore failed reading name hashes chunk from global container '%s'"), *GlobalContainerPath);
	//	return false;
	//}

	//LoadNameBatch(
	//	GlobalNameMap,
	//	TArrayView<const uint8>(GlobalNamesIoBuffer.ValueOrDie().Data(), GlobalNamesIoBuffer.ValueOrDie().DataSize()),
	//	TArrayView<const uint8>(GlobalNameHashesIoBuffer.ValueOrDie().Data(), GlobalNameHashesIoBuffer.ValueOrDie().DataSize()));

	//UE_LOG(LogPakAnalyzer, Display, TEXT("IoStore loading script imports..."));

	//TIoStatusOr<FIoBuffer> InitialLoadIoBuffer = GlobalIoStoreReader->Read(CreateIoChunkId(0, 0, EIoChunkType::LoaderInitialLoadMeta), FIoReadOptions());
	//if (!InitialLoadIoBuffer.IsOk())
	//{
	//	UE_LOG(LogPakAnalyzer, Warning, TEXT("IoStore failed reading initial load meta chunk from global container '%s'"), *GlobalContainerPath);
	//	return false;
	//}

	//FLargeMemoryReader InitialLoadArchive(InitialLoadIoBuffer.ValueOrDie().Data(), InitialLoadIoBuffer.ValueOrDie().DataSize());
	//int32 NumScriptObjects = 0;
	//InitialLoadArchive << NumScriptObjects;
	//const FScriptObjectEntry* ScriptObjectEntries = reinterpret_cast<const FScriptObjectEntry*>(InitialLoadIoBuffer.ValueOrDie().Data() + InitialLoadArchive.Tell());
	//for (int32 ScriptObjectIndex = 0; ScriptObjectIndex < NumScriptObjects; ++ScriptObjectIndex)
	//{
	//	const FScriptObjectEntry& ScriptObjectEntry = ScriptObjectEntries[ScriptObjectIndex];
	//	const FMappedName& MappedName = FMappedName::FromMinimalName(ScriptObjectEntry.ObjectName);
	//	check(MappedName.IsGlobal());
	//	FScriptObjectDesc& ScriptObjectDesc = ScriptObjectByGlobalIdMap.Add(ScriptObjectEntry.GlobalIndex);
	//	ScriptObjectDesc.Name = FName::CreateFromDisplayId(GlobalNameMap[MappedName.GetIndex()], MappedName.GetNumber());
	//	ScriptObjectDesc.GlobalImportIndex = ScriptObjectEntry.GlobalIndex;
	//	ScriptObjectDesc.OuterIndex = ScriptObjectEntry.OuterIndex;
	//}

	//for (auto& KV : ScriptObjectByGlobalIdMap)
	//{
	//	FScriptObjectDesc& ScriptObjectDesc = KV.Get<1>();
	//	if (ScriptObjectDesc.FullName.IsNone())
	//	{
	//		TArray<FScriptObjectDesc*> ScriptObjectStack;
	//		FScriptObjectDesc* Current = &ScriptObjectDesc;
	//		FString FullName;
	//		while (Current)
	//		{
	//			if (!Current->FullName.IsNone())
	//			{
	//				FullName = Current->FullName.ToString();
	//				break;
	//			}
	//			ScriptObjectStack.Push(Current);
	//			Current = ScriptObjectByGlobalIdMap.Find(Current->OuterIndex);
	//		}
	//		while (ScriptObjectStack.Num() > 0)
	//		{
	//			Current = ScriptObjectStack.Pop();
	//			FullName /= Current->Name.ToString();
	//			Current->FullName = FName(FullName);
	//		}
	//	}
	//}

	return true;
}

bool FIoStoreAnalyzer::InitializeReaders(const TArray<FString>& InPaks, const TArray<FString>& InDefaultAESKeys)
{
	static const EParallelForFlags ParallelForFlags = FPlatformMisc::IsDebuggerPresent() ? EParallelForFlags::ForceSingleThread : EParallelForFlags::Unbalanced;

	UE_LOG(LogPakAnalyzer, Display, TEXT("IoStore creating container readers..."));

	struct FChunkInfo
	{
		FIoChunkId ChunkId;
		int32 ReaderIndex;
	};

	TArray<FChunkInfo> AllChunkIds;
	for (int32 PakIndex = 0; PakIndex < InPaks.Num(); ++PakIndex)
	{
		const FString& ContainerFilePath = InPaks[PakIndex];
		const FString& DefaultAESKey = InDefaultAESKeys.IsValidIndex(PakIndex) ? InDefaultAESKeys[PakIndex] : TEXT("");
		FString DecryptKey;

		TSharedPtr<FIoStoreReader> Reader = CreateIoStoreReader(ContainerFilePath, DefaultAESKey, DecryptKey);
		if (!Reader.IsValid())
		{
			UE_LOG(LogPakAnalyzer, Warning, TEXT("Failed to read container '%s'"), *ContainerFilePath);
			continue;
		}

		FContainerInfo Info;
		Info.Reader = MoveTemp(Reader);
		Info.Summary.PakFilePath = ContainerFilePath;
		Info.Id = Info.Reader->GetContainerId();
		Info.EncryptionKeyGuid = Info.Reader->GetEncryptionKeyGuid();
		Info.Summary.MountPoint = Info.Reader->GetDirectoryIndexReader().GetMountPoint();
		EIoContainerFlags Flags = Info.Reader->GetContainerFlags();
		Info.bCompressed = bool(Flags & EIoContainerFlags::Compressed);
		Info.bEncrypted = bool(Flags & EIoContainerFlags::Encrypted);
		Info.bSigned = bool(Flags & EIoContainerFlags::Signed);
		Info.bIndexed = bool(Flags & EIoContainerFlags::Indexed);
		Info.Summary.PakInfo.bEncryptedIndex = Info.bEncrypted;
		Info.Summary.PakInfo.EncryptionKeyGuid = Info.EncryptionKeyGuid;
		Info.Summary.DecryptAESKeyStr = DecryptKey;
		if (!FBase64::Decode(*DecryptKey, DecryptKey.Len(), Info.Summary.DecryptAESKey.Key))
		{
			Info.Summary.DecryptAESKey.Reset();
		}

		FIoStoreTocResourceInfo* TocResource = TocResources.Find(Info.Id.Value());
		if (TocResource)
		{
			for (const FIoChunkId& ChunkId : TocResource->ChunkIds)
			{
				AllChunkIds.Add({ ChunkId, StoreContainers.Num()});
			}

			TArray<FString> CompressionMethods;
			for (FName CompressionName : TocResource->CompressionMethods)
			{
				CompressionMethods.Add(CompressionName.ToString());
			}
			Info.Summary.CompressionMethods = FString::Join(CompressionMethods, TEXT(", "));

			const int64 CasFileSize = IPlatformFile::GetPlatformPhysical().FileSize(*Info.Summary.PakFilePath);
			Info.Summary.PakFileSize = CasFileSize + TocResource->TocFileSize;
			Info.Summary.PakInfo.IndexOffset = CasFileSize;
		}

		TIoStatusOr<FIoBuffer> IoBuffer = Info.Reader->Read(CreateIoChunkId(Info.Reader->GetContainerId().Value(), 0, EIoChunkType::ContainerHeader), FIoReadOptions());
		if (IoBuffer.IsOk())
		{
			FMemoryReaderView Ar(MakeArrayView(IoBuffer.ValueOrDie().Data(), IoBuffer.ValueOrDie().DataSize()));
			FIoContainerHeader ContainerHeader;
			
			// FArchive& operator<<(FArchive& Ar, FIoContainerHeader& ContainerHeader) in IoContainerHeader.cpp

			uint32 Signature = FIoContainerHeader::Signature;
			Ar << Signature;
			if (Signature != FIoContainerHeader::Signature)
			{
				UE_LOG(LogPakAnalyzer, Warning, TEXT("Failed to read container header '%s', signature not match!"), *ContainerFilePath);
				continue;
			}

			EIoContainerHeaderVersion Version = EIoContainerHeaderVersion::Latest;
			Ar << Version;
			Ar << ContainerHeader.ContainerId;
			if (Version <= EIoContainerHeaderVersion::LocalizedPackages)
			{
				uint32 PackageCount = 0;
				Ar << PackageCount;
			}

			Ar << ContainerHeader.PackageIds;
			Ar << ContainerHeader.StoreEntries;
			if (Version >= EIoContainerHeaderVersion::OptionalSegmentPackages)
			{
				Ar << ContainerHeader.OptionalSegmentPackageIds;
				Ar << ContainerHeader.OptionalSegmentStoreEntries;
			}
			ContainerHeader.RedirectsNameMap = LoadNameBatch(Ar);
			Ar << ContainerHeader.LocalizedPackages;
			Ar << ContainerHeader.PackageRedirects;

			// RawCulturePackageMap = ContainerHeader.CulturePackageMap;
			// RawPackageRedirects = ContainerHeader.PackageRedirects;

			TArrayView<FFilePackageStoreEntry> StoreEntries(reinterpret_cast<FFilePackageStoreEntry*>(ContainerHeader.StoreEntries.GetData()), ContainerHeader.PackageIds.Num());

			for (int32 PackageIndex = 0; PackageIndex < StoreEntries.Num(); ++PackageIndex)
			{
				Info.StoreEntryMap.Add(ContainerHeader.PackageIds[PackageIndex], FPackageStoreExportEntry(StoreEntries[PackageIndex]));
			}
		}

		StoreContainers.Add(Info);
	}

	UE_LOG(LogPakAnalyzer, Display, TEXT("IoStore parsing packages..."));

	PackageInfos.Empty(AllChunkIds.Num());
	PackageInfos.AddZeroed(AllChunkIds.Num());
	ParallelFor(AllChunkIds.Num(), [this, &AllChunkIds](int32 Index)
	{
		const FIoChunkId& ChunkId = AllChunkIds[Index].ChunkId;

		FPackageId PackageId;
		EIoChunkType ChunkType;
		ParseChunkInfo(ChunkId, PackageId, ChunkType);

		//if (ChunkType == EIoChunkType::ExportBundleData || ChunkType == EIoChunkType::BulkData ||
		//	ChunkType == EIoChunkType::OptionalBulkData || ChunkType == EIoChunkType::MemoryMappedBulkData)
		{
			FStorePackageInfo& PackageInfo = PackageInfos[Index];
			PackageInfo.PackageId = PackageId;
			PackageInfo.ContainerIndex = AllChunkIds[Index].ReaderIndex;
			PackageInfo.ChunkType = ChunkType;
			PackageInfo.ChunkId = ChunkId;

			switch (ChunkType)
			{
			case EIoChunkType::ExportBundleData: PackageInfo.Extension = TEXT("uexp"); break;
			case EIoChunkType::BulkData: PackageInfo.Extension = TEXT("ubulk"); break;
			case EIoChunkType::OptionalBulkData: PackageInfo.Extension = TEXT("uptnl"); break;
			case EIoChunkType::MemoryMappedBulkData: PackageInfo.Extension = TEXT("umappedbulk"); break;
			case EIoChunkType::ScriptObjects: PackageInfo.Extension = TEXT("ScriptObjects"); break;
			case EIoChunkType::ContainerHeader: PackageInfo.Extension = TEXT("ContainerHeader"); break;
			case EIoChunkType::ExternalFile: PackageInfo.Extension = TEXT("ExternalFile"); break;
			case EIoChunkType::ShaderCodeLibrary: PackageInfo.Extension = TEXT("ShaderCodeLibrary"); break;
			case EIoChunkType::ShaderCode: PackageInfo.Extension = TEXT("ShaderCode"); break;
			case EIoChunkType::PackageStoreEntry: PackageInfo.Extension = TEXT("PackageStoreEntry"); break;
			case EIoChunkType::DerivedData: PackageInfo.Extension = TEXT("DerivedData"); break;
			case EIoChunkType::EditorDerivedData: PackageInfo.Extension = TEXT("EditorDerivedData"); break;
			case EIoChunkType::PackageResource: PackageInfo.Extension = TEXT("PackageResource"); break;
			default: PackageInfo.Extension = TEXT("");
			}
		}
	}, ParallelForFlags);

	UE_LOG(LogPakAnalyzer, Display, TEXT("IoStore loading package FNames..."));

	ParallelFor(PackageInfos.Num(), [this](int32 Index)
	{
		FStorePackageInfo& PackageInfo = PackageInfos[Index];
		if (!PackageInfo.PackageId.IsValid())
		{
			return;
		}

		FContainerInfo& ContainerInfo = StoreContainers[PackageInfo.ContainerIndex];
		TSharedPtr<FIoStoreReader>& Reader = ContainerInfo.Reader;

		TIoStatusOr<FIoStoreTocChunkInfo> ChunkInfo = Reader->GetChunkInfo(PackageInfo.ChunkId);
		PackageInfo.ChunkInfo = FIoStoreTocChunkInfo(ChunkInfo.ValueOrDie());

		FIoStoreTocResourceInfo* TocResource = TocResources.Find(ContainerInfo.Id.Value());
		if (TocResource)
		{
			FillPackageInfo(*TocResource, PackageInfo);
		}

		if (PackageInfo.ChunkType == EIoChunkType::ExportBundleData)
		{
			FIoReadOptions ReadOptions;
			TIoStatusOr<FIoBuffer> IoBuffer = Reader->Read(PackageInfo.ChunkId, ReadOptions);

			const uint8* PackageSummaryData = IoBuffer.ValueOrDie().Data();
			const FZenPackageSummary* PackageSummary = reinterpret_cast<const FZenPackageSummary*>(PackageSummaryData);
			if (PackageSummary->HeaderSize > IoBuffer.ValueOrDie().DataSize())
			{
				ReadOptions.SetRange(0, PackageSummary->HeaderSize);
				IoBuffer = Reader->Read(PackageInfo.ChunkId, ReadOptions);
				PackageSummaryData = IoBuffer.ValueOrDie().Data();
				PackageSummary = reinterpret_cast<const FZenPackageSummary*>(PackageSummaryData);
			}

			TArrayView<const uint8> HeaderDataView(PackageSummaryData + sizeof(FZenPackageSummary), PackageSummary->HeaderSize - sizeof(FZenPackageSummary));
			FMemoryReaderView HeaderDataReader(HeaderDataView);

			FZenPackageVersioningInfo VersioningInfo;
			if (PackageSummary->bHasVersioningInfo)
			{
				HeaderDataReader << VersioningInfo;
			}

			FZenPackageCellOffsets CellOffsets;
			if (!PackageSummary->bHasVersioningInfo || VersioningInfo.PackageVersion >= EUnrealEngineObjectUE5Version::VERSE_CELLS)
			{
				HeaderDataReader << CellOffsets.CellImportMapOffset;
				HeaderDataReader << CellOffsets.CellExportMapOffset;
			}
			else
			{
				CellOffsets.CellImportMapOffset = PackageSummary->ExportBundleEntriesOffset;
				CellOffsets.CellExportMapOffset = PackageSummary->ExportBundleEntriesOffset;
			}
			
			TArray<FDisplayNameEntryId> PackageNameMap;
			{
				PackageNameMap = LoadNameBatch(HeaderDataReader);
			}

			PackageInfo.CookedHeaderSize = PackageSummary->CookedHeaderSize;
			PackageInfo.PackageName = PackageNameMap[PackageSummary->Name.GetIndex()].ToName(PackageSummary->Name.GetNumber());
			PackageInfo.ImportedPublicExportHashes = MakeArrayView<const uint64>(reinterpret_cast<const uint64*>(PackageSummaryData + PackageSummary->ImportedPublicExportHashesOffset), (PackageSummary->ImportMapOffset - PackageSummary->ImportedPublicExportHashesOffset) / sizeof(uint64));
			
			const FPackageObjectIndex* ImportMap = reinterpret_cast<const FPackageObjectIndex*>(PackageSummaryData + PackageSummary->ImportMapOffset);
			PackageInfo.Imports.SetNum((PackageSummary->ExportMapOffset - PackageSummary->ImportMapOffset) / sizeof(FPackageObjectIndex));
			for (int32 ImportIndex = 0; ImportIndex < PackageInfo.Imports.Num(); ++ImportIndex)
			{
				FIoStoreImport& Import = PackageInfo.Imports[ImportIndex];
				Import.GlobalImportIndex = ImportMap[ImportIndex];
			}
			
			const FPackageStoreExportEntry* ExportEntry = ContainerInfo.StoreEntryMap.Find(PackageInfo.PackageId);
			if (ExportEntry)
			{
				PackageInfo.DependencyPackages = ExportEntry->DependencyPackages;
				const FExportMapEntry* ExportMap = reinterpret_cast<const FExportMapEntry*>(PackageSummaryData + PackageSummary->ExportMapOffset);
				PackageInfo.Exports.SetNum((PackageSummary->ExportBundleEntriesOffset - PackageSummary->ExportMapOffset) / sizeof(FExportMapEntry));
				for (int32 ExportIndex = 0; ExportIndex < PackageInfo.Exports.Num(); ++ExportIndex)
				{
					const FExportMapEntry& ExportMapEntry = ExportMap[ExportIndex];
					FIoStoreExport& Export = PackageInfo.Exports[ExportIndex];
					Export.Name = PackageNameMap[ExportMapEntry.ObjectName.GetIndex()].ToName(ExportMapEntry.ObjectName.GetNumber());
					Export.PublicExportHash = ExportMapEntry.PublicExportHash;
					Export.OuterIndex = ExportMapEntry.OuterIndex;
					Export.ClassIndex = ExportMapEntry.ClassIndex;
					Export.SuperIndex = ExportMapEntry.SuperIndex;
					Export.TemplateIndex = ExportMapEntry.TemplateIndex;
					//Export.GlobalImportIndex = ExportMapEntry.GlobalImportIndex;
					Export.SerialSize = ExportMapEntry.CookedSerialSize;
					Export.SerialOffset = ExportMapEntry.CookedSerialOffset;
					Export.ObjectFlags = ExportMapEntry.ObjectFlags;
					Export.FilterFlags = ExportMapEntry.FilterFlags;
					Export.Package = &PackageInfo;
				}
			}

			PackageInfo.AssetSummary = MakeShared<FAssetSummary>();
			FPackageFileSummary& AssetPackageSummary = PackageInfo.AssetSummary->PackageSummary;
			FMemory::Memzero(&AssetPackageSummary, sizeof(AssetPackageSummary));

			AssetPackageSummary.Tag = PACKAGE_FILE_TAG;
			//AssetPackageSummary.PackageFlags = PackageSummary->PackageFlags;
			AssetPackageSummary.SetPackageFlags(PackageSummary->PackageFlags);
			AssetPackageSummary.TotalHeaderSize = PackageSummary->CookedHeaderSize;

			// FNames
			PackageInfo.AssetSummary->Names.SetNum(PackageNameMap.Num());
			AssetPackageSummary.NameCount = PackageNameMap.Num();
			AssetPackageSummary.NameOffset = 0;
			for (int32 i = 0; i < PackageNameMap.Num(); ++i)
			{
				PackageInfo.AssetSummary->Names[i] = MakeShared<FName>(PackageNameMap[i].ToName(0));
			}

			// Imports
			AssetPackageSummary.ImportCount = PackageInfo.Imports.Num();
			AssetPackageSummary.ImportOffset = 0;
			PackageInfo.AssetSummary->ObjectImports.SetNum(AssetPackageSummary.ImportCount);

			// Exports
			AssetPackageSummary.ExportCount = PackageInfo.Exports.Num();
			AssetPackageSummary.ExportOffset = 0;
			PackageInfo.AssetSummary->ObjectExports.SetNum(AssetPackageSummary.ExportCount);

			//const FExportBundleHeader* ExportBundleHeaders = reinterpret_cast<const FExportBundleHeader*>(PackageSummaryData + PackageSummary->ExportBundlesOffset);
			//const FExportBundleEntry* ExportBundleEntries = reinterpret_cast<const FExportBundleEntry*>(ExportBundleHeaders + Job.PackageDesc->ExportBundleCount);
			//uint64 CurrentExportOffset = PackageSummarySize;
			//for (int32 ExportBundleIndex = 0; ExportBundleIndex < Job.PackageDesc->ExportBundleCount; ++ExportBundleIndex)
			//{
			//	TArray<FExportBundleEntryDesc>& ExportBundleDesc = Job.PackageDesc->ExportBundles.AddDefaulted_GetRef();
			//	const FExportBundleHeader* ExportBundle = ExportBundleHeaders + ExportBundleIndex;
			//	const FExportBundleEntry* BundleEntry = ExportBundleEntries + ExportBundle->FirstEntryIndex;
			//	const FExportBundleEntry* BundleEntryEnd = BundleEntry + ExportBundle->EntryCount;
			//	check(BundleEntry <= BundleEntryEnd);
			//	while (BundleEntry < BundleEntryEnd)
			//	{
			//		FExportBundleEntryDesc& EntryDesc = ExportBundleDesc.AddDefaulted_GetRef();
			//		EntryDesc.CommandType = FExportBundleEntry::EExportCommandType(BundleEntry->CommandType);
			//		EntryDesc.LocalExportIndex = BundleEntry->LocalExportIndex;
			//		EntryDesc.Export = &Job.PackageDesc->Exports[BundleEntry->LocalExportIndex];
			//		if (BundleEntry->CommandType == FExportBundleEntry::ExportCommandType_Serialize)
			//		{
			//			EntryDesc.Export->SerialOffset = CurrentExportOffset;
			//			CurrentExportOffset += EntryDesc.Export->SerialSize;

			//			if (bIncludeExportHashes)
			//			{
			//				check(EntryDesc.Export->SerialOffset + EntryDesc.Export->SerialSize <= IoBuffer.ValueOrDie().DataSize());
			//				FSHA1::HashBuffer(IoBuffer.ValueOrDie().Data() + EntryDesc.Export->SerialOffset, EntryDesc.Export->SerialSize, EntryDesc.Export->Hash.Hash);
			//			}
			//		}
			//		++BundleEntry;
			//	}
			//}
		}
	}, ParallelForFlags);

	UE_LOG(LogPakAnalyzer, Display, TEXT("IoStore assigning package name..."));

	TMap<FPackageId, FName> PackageNameMap;
	for (const FStorePackageInfo& PackageInfo : PackageInfos)
	{
		if (PackageInfo.PackageName != NAME_None)
		{
			PackageNameMap.Add(PackageInfo.PackageId, PackageInfo.PackageName);
		}

		//if (PackageInfo.PackageId.IsValid())
		//{
		//	for (const FIoStoreExport& Export : PackageInfo.Exports)
		//	{
		//		if (!Export.GlobalImportIndex.IsNull())
		//		{
		//			ExportByGlobalIdMap.Add(Export.GlobalImportIndex, &Export);
		//		}
		//	}
		//}
	}

	ParallelFor(PackageInfos.Num(), [this, &PackageNameMap](int32 Index)
	{
		FStorePackageInfo& PackageInfo = PackageInfos[Index];
		if (!PackageInfo.PackageId.IsValid())
		{
			return;
		}

		if (PackageInfo.PackageName == NAME_None)
		{
			if (FName* PackageName = PackageNameMap.Find(PackageInfo.PackageId))
			{
				PackageInfo.PackageName = *PackageName;
			}
			else
			{
				PackageInfo.PackageName = *FString::Printf(TEXT("%I64u"), PackageInfo.PackageId.Value());
			}
		}
	}, ParallelForFlags);

	UE_LOG(LogPakAnalyzer, Display, TEXT("Connecting imports and exports..."));

	TMap<FPublicExportKey, FIoStoreExport*> ExportByKeyMap;
	for (FStorePackageInfo& PackageInfo : PackageInfos)
	{
		for (FIoStoreExport& ExportDesc : PackageInfo.Exports)
		{
			if (ExportDesc.PublicExportHash)
			{
				FPublicExportKey Key = FPublicExportKey::MakeKey(PackageInfo.PackageId, ExportDesc.PublicExportHash);
				ExportByKeyMap.Add(Key, &ExportDesc);
			}
		}
	}
	
	ParallelFor(PackageInfos.Num(), [this](int32 Index)
	{
		FStorePackageInfo& PackageInfo = PackageInfos[Index];
		if (!PackageInfo.PackageId.IsValid())
		{
			return;
		}

		for (int32 i = 0; i < PackageInfo.Exports.Num(); ++i)
		{
			FIoStoreExport& Export = PackageInfo.Exports[i];
			if (Export.FullName.IsNone())
			{
				TArray<FIoStoreExport*> ExportStack;

				FIoStoreExport* Current = &Export;
				TStringBuilder<2048> FullNameBuilder;
				TCHAR NameBuffer[FName::StringBufferSize];
				for (;;)
				{
					if (!Current->FullName.IsNone())
					{
						Current->FullName.ToString(NameBuffer);
						FullNameBuilder.Append(NameBuffer);
						break;
					}
					ExportStack.Push(Current);
					if (Current->OuterIndex.IsNull())
					{
						PackageInfo.PackageName.ToString(NameBuffer);
						FullNameBuilder.Append(NameBuffer);
						break;
					}
					Current = &PackageInfo.Exports[Current->OuterIndex.Value()];
				}
				while (ExportStack.Num() > 0)
				{
					Current = ExportStack.Pop(false);
					FullNameBuilder.Append(TEXT("/"));
					Current->Name.ToString(NameBuffer);
					FullNameBuilder.Append(NameBuffer);
					Current->FullName = FName(FullNameBuilder);
				}
			}
		}
	}, ParallelForFlags);

	UE_LOG(LogPakAnalyzer, Display, TEXT("Filling imports and exports..."));

	TMultiMap<FString, FString> DependsMap;
	FCriticalSection Mutex;

	ParallelFor(PackageInfos.Num(), [this, &PackageNameMap, &DependsMap, &Mutex, &ExportByKeyMap](int32 Index)
	{
		FStorePackageInfo& PackageInfo = PackageInfos[Index];
		if (!PackageInfo.PackageId.IsValid() || !PackageInfo.AssetSummary.IsValid())
		{
			return;
		}

		FName MainObjectName = *FPaths::GetBaseFilename(PackageInfo.PackageName.ToString());
		FName MainClassObjectName = *FString::Printf(TEXT("%s_C"), *MainObjectName.ToString());
		FName MainObjectClassName = NAME_None;
		FName MainClassObjectClassName = NAME_None;
		FName AssetClass = NAME_None;

		for (int32 i = 0; i < PackageInfo.Exports.Num(); ++i)
		{
			FIoStoreExport& Export = PackageInfo.Exports[i];

			FObjectExportPtrType ObjectExport = MakeShared<FObjectExportEx>();
			ObjectExport->Index = i;
			ObjectExport->ObjectName = Export.Name;
			ObjectExport->SerialSize = Export.SerialSize;
			ObjectExport->SerialOffset = Export.SerialOffset;
			ObjectExport->bIsAsset = (Export.ObjectFlags & RF_Public) && !(Export.ObjectFlags & (RF_Transient | RF_ClassDefaultObject));
			ObjectExport->bNotForClient = Export.FilterFlags == EExportFilterFlags::NotForClient;
			ObjectExport->bNotForServer = Export.FilterFlags == EExportFilterFlags::NotForServer;
			ObjectExport->ClassName = FindObjectName(Export.ClassIndex, &PackageInfo);
			ObjectExport->Super = FindObjectName(Export.SuperIndex, &PackageInfo);
			ObjectExport->TemplateObject = FindObjectName(Export.TemplateIndex, &PackageInfo);
			ObjectExport->ObjectPath = Export.FullName;
			ObjectExport->DependencyList.SetNum(0);

			PackageInfo.AssetSummary->ObjectExports[i] = ObjectExport;

			FName ObjectClass = *FPaths::GetBaseFilename(ObjectExport->ClassName.ToString());
			FName ObjectName = *FPaths::GetBaseFilename(ObjectExport->ObjectName.ToString());
			if (ObjectName == MainObjectName)
			{
				MainObjectClassName = ObjectClass;
			}
			else if (ObjectName == MainClassObjectName)
			{
				MainClassObjectClassName = ObjectClass;
			}

			if (ObjectExport->bIsAsset)
			{
				AssetClass = ObjectClass;
			}
		}

		if (MainObjectClassName == NAME_None && MainClassObjectClassName == NAME_None)
		{
			if (PackageInfo.AssetSummary->ObjectExports.Num() == 1)
			{
				MainObjectClassName = *FPaths::GetBaseFilename(PackageInfo.AssetSummary->ObjectExports[0]->ClassName.ToString());
			}
			else if (!AssetClass.IsNone())
			{
				MainObjectClassName = AssetClass;
			}
		}

		if (MainObjectClassName != NAME_None || MainClassObjectClassName != NAME_None)
		{
			PackageInfo.DefaultClassName = MainObjectClassName != NAME_None ? MainObjectClassName : MainClassObjectClassName;
		}

		for (int32 i = 0; i < PackageInfo.Imports.Num(); ++i)
		{
			FName ImportClassName;
			FIoStoreImport& Import = PackageInfo.Imports[i];
			if (!Import.GlobalImportIndex.IsNull())
			{
				if (Import.GlobalImportIndex.IsPackageImport())
				{
					FPublicExportKey Key = FPublicExportKey::FromPackageImport(Import.GlobalImportIndex, PackageInfo.DependencyPackages, PackageInfo.ImportedPublicExportHashes);
					FIoStoreExport* Export = ExportByKeyMap.FindRef(Key);
					if (!Export)
					{
						Import.Name = *FString::Printf(TEXT("Missing import: 0x%llX"), Import.GlobalImportIndex.Value());
						//UE_LOG(LogIoStore, Warning, TEXT("Missing import: 0x%llX in package 0x%llX '%s'"), Import.GlobalImportIndex.Value(), PackageDesc->PackageId.ValueForDebugging(), *PackageDesc->PackageName.ToString());
					}
					else
					{
						Import.Name = Export->FullName;
						ImportClassName = FindObjectName(Export->ClassIndex, Export->Package);
					}
				}
				else
				{
					FScriptObjectDesc* ScriptObjectDesc = ScriptObjectByGlobalIdMap.Find(Import.GlobalImportIndex);
					if (ScriptObjectDesc)
					{
						Import.Name = ScriptObjectDesc->FullName;
					}
					else
					{
						Import.Name = *FString::Printf(TEXT("Missing Script Object for Import: 0x%llX"), Import.GlobalImportIndex.Value());
						//UE_LOG(LogIoStore, Warning, TEXT("Missing Script Object for Import: 0x%llX in package 0x%llX '%s'"), Import.GlobalImportIndex.Value(), PackageDesc->PackageId.ValueForDebugging(), *PackageDesc->PackageName.ToString());
					}
				}
			}

			FObjectImportPtrType ObjectImport = MakeShared<FObjectImportEx>();
			ObjectImport->Index = i;
			ObjectImport->ObjectPath = Import.Name;
			ObjectImport->ObjectName = *FPaths::GetBaseFilename(ObjectImport->ObjectPath.ToString());
			ObjectImport->ClassName = ImportClassName;

			PackageInfo.AssetSummary->ObjectImports[i] = ObjectImport;
		}

		PackageInfo.AssetSummary->DependencyList.SetNum(PackageInfo.DependencyPackages.Num());
		for (int32 i = 0; i < PackageInfo.DependencyPackages.Num(); ++i)
		{
			FPackageInfoPtr DependencyPackage = MakeShared<struct FPackageInfo>();
			if (FName* PackageName = PackageNameMap.Find(PackageInfo.DependencyPackages[i]))
			{
				DependencyPackage->PackageName = *PackageName;

				FScopeLock ScopeLock(&Mutex);
				DependsMap.Add(DependencyPackage->PackageName.ToString().ToLower(), PackageInfo.PackageName.ToString());
			}
			else
			{
				DependencyPackage->PackageName = *FString::Printf(TEXT("Missing package: 0x%X, may be in other ucas!"), PackageInfo.DependencyPackages[i].ValueForDebugging());
			}

			PackageInfo.AssetSummary->DependencyList[i] = DependencyPackage;
		}
	}, ParallelForFlags);

	UE_LOG(LogPakAnalyzer, Display, TEXT("Parsing dependents..."));

	ParallelFor(PackageInfos.Num(), [this, &DependsMap](int32 Index) {
		FStorePackageInfo& PackageInfo = PackageInfos[Index];
		if (!PackageInfo.PackageId.IsValid() || !PackageInfo.AssetSummary.IsValid())
		{
			return;
		}

		TArray<FString> Assets;
		DependsMap.MultiFind(PackageInfo.PackageName.ToString().ToLower(), Assets);

		for (const FString& Asset : Assets)
		{
			FPackageInfoPtr Depends = MakeShared<FPackageInfo>();
			Depends->PackageName = *Asset;
			PackageInfo.AssetSummary->DependentList.Add(Depends);
		}
	}, ParallelForFlags);

	UE_LOG(LogPakAnalyzer, Display, TEXT("IoStore creating container readers finish."));

	return true;
}

bool FIoStoreAnalyzer::PreLoadIoStore(const FString& InTocPath, const FString& InCasPath, const FString& InDefaultAESKey, TMap<FGuid, FAES::FAESKey>& OutKeys, FString& OutDecryptKey)
{
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();

	TUniquePtr<IFileHandle>	TocFileHandle(PlatformFile.OpenRead(*InTocPath, /* allowwrite */ false));
	if (!TocFileHandle.IsValid())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Preload toc file failed! Path: %s."), *InTocPath);
		return false;
	}

	// header
	FIoStoreTocHeader Header;
	if (!TocFileHandle->Read(reinterpret_cast<uint8*>(&Header), sizeof(FIoStoreTocHeader)))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Preload toc file failed! Read toc header failed! Path: %s."), *InTocPath);
		return false;
	}

	if (!Header.CheckMagic())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Preload toc file failed! TOC header magic mismatch! Path: %s."), *InTocPath);
		return false;
	}

	if (Header.TocHeaderSize != sizeof(FIoStoreTocHeader))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Preload toc file failed! TOC header size mismatch! Path: %s."), *InTocPath);
		return false;
	}

	if (Header.TocCompressedBlockEntrySize != sizeof(FIoStoreTocCompressedBlockEntry))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Preload toc file failed! TOC compressed block entry size mismatch! Path: %s."), *InTocPath);
		return false;
	}

	if (Header.Version < static_cast<uint8>(EIoStoreTocVersion::DirectoryIndex))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Preload toc file failed! TOC header version outdated! Path: %s."), *InTocPath);
		return false;
	}

	if (Header.Version > static_cast<uint8>(EIoStoreTocVersion::Latest))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Preload toc file failed! TOC header version too new! Path: %s."), *InTocPath);
		return false;
	}

	const uint64 TotalTocSize = TocFileHandle->Size() - sizeof(FIoStoreTocHeader);
	TUniquePtr<uint8[]> TocBuffer = MakeUnique<uint8[]>(TotalTocSize);
	if (!TocFileHandle->Read(TocBuffer.Get(), TotalTocSize))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Preload toc file failed! Failed to read IoStore TOC file! Path: %s."), *InTocPath);
		return false;
	}

	FIoStoreTocResourceInfo TocResource;
	TocResource.TocFileSize = TocFileHandle->Size();
	TocResource.Header = Header;

	// Chunk IDs
	const uint8* DataPtr = TocBuffer.Get();
	const FIoChunkId* ChunkIds = reinterpret_cast<const FIoChunkId*>(DataPtr);
	TocResource.ChunkIds = MakeArrayView<FIoChunkId const>(ChunkIds, Header.TocEntryCount);
	if (TocResource.ChunkIds.Num() <= 0)
	{
		return false;
	}

	for (int32 ChunkIndex = 0; ChunkIndex < TocResource.ChunkIds.Num(); ++ChunkIndex)
	{
		int32 PerfectHashSeedIndex = TocResource.ChunkPerfectHashSeeds.IsValidIndex(ChunkIndex) ? TocResource.ChunkPerfectHashSeeds[ChunkIndex] : INDEX_NONE;

		if (PerfectHashSeedIndex != INDEX_NONE)
		{
			// If a perfect hash seed exists, use it for indexing
			int32 HashIndex = TocResource.ChunkPerfectHashSeeds[ChunkIndex];
		}
		else
		{
			// If no perfect hash seed, fall back to a standard index lookup
			int32 FallbackIndex = TocResource.ChunkIndicesWithoutPerfectHash.Find(ChunkIndex);
		}
	}

	DataPtr += Header.TocEntryCount * sizeof(FIoChunkId);

	// Chunk offsets
	TArray<FIoOffsetAndLength> ChunkOffsetLengthsArray;
	const FIoOffsetAndLength* ChunkOffsetLengths = reinterpret_cast<const FIoOffsetAndLength*>(DataPtr);
	ChunkOffsetLengthsArray = MakeArrayView<FIoOffsetAndLength const>(ChunkOffsetLengths, Header.TocEntryCount);
	DataPtr += Header.TocEntryCount * sizeof(FIoOffsetAndLength);

	// Chunk perfect hash map
	uint32 PerfectHashSeedsCount = 0;
	uint32 ChunksWithoutPerfectHashCount = 0;
	if (Header.Version >= static_cast<uint8>(EIoStoreTocVersion::PerfectHashWithOverflow))
	{
		PerfectHashSeedsCount = Header.TocChunkPerfectHashSeedsCount;
		ChunksWithoutPerfectHashCount = Header.TocChunksWithoutPerfectHashCount;
	}
	else if (Header.Version >= static_cast<uint8>(EIoStoreTocVersion::PerfectHash))
	{
		PerfectHashSeedsCount = Header.TocChunkPerfectHashSeedsCount;
	}
	if (PerfectHashSeedsCount)
	{
		const int32* ChunkPerfectHashSeeds = reinterpret_cast<const int32*>(DataPtr);
		//OutTocResource.ChunkPerfectHashSeeds = MakeArrayView<int32 const>(ChunkPerfectHashSeeds, PerfectHashSeedsCount);
		DataPtr += PerfectHashSeedsCount * sizeof(int32);
	}
	if (ChunksWithoutPerfectHashCount)
	{
		const int32* ChunkIndicesWithoutPerfectHash = reinterpret_cast<const int32*>(DataPtr);
		//OutTocResource.ChunkIndicesWithoutPerfectHash = MakeArrayView<int32 const>(ChunkIndicesWithoutPerfectHash, ChunksWithoutPerfectHashCount);
		DataPtr += ChunksWithoutPerfectHashCount * sizeof(int32);
	}

	// Compression blocks
	const FIoStoreTocCompressedBlockEntry* CompressionBlocks = reinterpret_cast<const FIoStoreTocCompressedBlockEntry*>(DataPtr);
	TocResource.CompressionBlocks = MakeArrayView<FIoStoreTocCompressedBlockEntry const>(CompressionBlocks, Header.TocCompressedBlockEntryCount);
	DataPtr += Header.TocCompressedBlockEntryCount * sizeof(FIoStoreTocCompressedBlockEntry);

	// Compression methods
	TocResource.CompressionMethods.Reserve(Header.CompressionMethodNameCount + 1);
	TocResource.CompressionMethods.Add(NAME_None);

	const ANSICHAR* AnsiCompressionMethodNames = reinterpret_cast<const ANSICHAR*>(DataPtr);
	for (uint32 CompressonNameIndex = 0; CompressonNameIndex < Header.CompressionMethodNameCount; CompressonNameIndex++)
	{
		const ANSICHAR* AnsiCompressionMethodName = AnsiCompressionMethodNames + CompressonNameIndex * Header.CompressionMethodNameLength;
		TocResource.CompressionMethods.Add(FName(AnsiCompressionMethodName));
	}
	DataPtr += Header.CompressionMethodNameCount * Header.CompressionMethodNameLength;

	// Chunk block signatures
	const uint8* SignatureBuffer = reinterpret_cast<const uint8*>(DataPtr);
	const uint8* DirectoryIndexBuffer = SignatureBuffer;

	const bool bIsSigned = EnumHasAnyFlags(Header.ContainerFlags, EIoContainerFlags::Signed);
	if (bIsSigned)
	{
		const int32* HashSize = reinterpret_cast<const int32*>(SignatureBuffer);
		TArrayView<const uint8> TocSignature = MakeArrayView<const uint8>(reinterpret_cast<const uint8*>(HashSize + 1), *HashSize);
		TArrayView<const uint8> BlockSignature = MakeArrayView<const uint8>(TocSignature.GetData() + *HashSize, *HashSize);
		TArrayView<const uint8> BothSignatures = MakeArrayView<const uint8>(TocSignature.GetData(), *HashSize * 2);
		//FSHA1::HashBuffer(BothSignatures.GetData(), BothSignatures.Num(), OutTocResource.SignatureHash.Hash);
		TArrayView<const FSHAHash> ChunkBlockSignatures = MakeArrayView<const FSHAHash>(reinterpret_cast<const FSHAHash*>(BlockSignature.GetData() + *HashSize), Header.TocCompressedBlockEntryCount);

		// Adjust address to meta data
		DirectoryIndexBuffer = reinterpret_cast<const uint8*>(ChunkBlockSignatures.GetData() + ChunkBlockSignatures.Num());

		//OutTocResource.ChunkBlockSignatures = ChunkBlockSignatures;
	}

	// Directory index
	//if (EnumHasAnyFlags(ReadOptions, EIoStoreTocReadOptions::ReadDirectoryIndex) &&
	//	EnumHasAnyFlags(Header.ContainerFlags, EIoContainerFlags::Indexed) &&
	//	Header.DirectoryIndexSize > 0)
	//{
	//	OutTocResource.DirectoryIndexBuffer = MakeArrayView<const uint8>(DirectoryIndexBuffer, Header.DirectoryIndexSize);
	//}

	// Meta
	const uint8* TocMeta = DirectoryIndexBuffer + Header.DirectoryIndexSize;
	const FIoStoreTocEntryMeta* ChunkMetas = reinterpret_cast<const FIoStoreTocEntryMeta*>(TocMeta);
	TocResource.ChunkMetas = MakeArrayView<FIoStoreTocEntryMeta const>(ChunkMetas, Header.TocEntryCount);

	if (Header.Version < static_cast<uint8>(EIoStoreTocVersion::PartitionSize))
	{
		Header.PartitionCount = 1;
		Header.PartitionSize = MAX_uint64;
	}

	bool bShouldLoad = true;
	if (Header.EncryptionKeyGuid.IsValid() || EnumHasAnyFlags(Header.ContainerFlags, EIoContainerFlags::Encrypted))
	{
		OutDecryptKey = InDefaultAESKey;
		FAES::FAESKey AESKey;

		bShouldLoad = InDefaultAESKey.IsEmpty() ? false : TryDecryptIoStore(TocResource, ChunkOffsetLengthsArray[0], TocResource.ChunkMetas[0], InCasPath, InDefaultAESKey, AESKey);

		if (!bShouldLoad)
		{
			if (FPakAnalyzerDelegates::OnGetAESKey.IsBound())
			{
				bool bCancel = true;
				do
				{
					OutDecryptKey = FPakAnalyzerDelegates::OnGetAESKey.Execute(InCasPath, Header.EncryptionKeyGuid, bCancel);

					bShouldLoad = !bCancel ? TryDecryptIoStore(TocResource, ChunkOffsetLengthsArray[0], TocResource.ChunkMetas[0], InCasPath, OutDecryptKey, AESKey) : false;
				} while (!bShouldLoad && !bCancel);
			}
			else
			{
				UE_LOG(LogPakAnalyzer, Error, TEXT("Can't open encrypt iostore without OnGetAESKey bound!"));
				FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Can't open encrypt iostore without OnGetAESKey bound!")));
				bShouldLoad = false;
			}
		}

		if (!bShouldLoad)
		{
			return false;
		}

		OutKeys.Add(Header.EncryptionKeyGuid, AESKey);
	}

	TocResources.Add(Header.ContainerId.Value(), TocResource);

	return true;
}

bool FIoStoreAnalyzer::TryDecryptIoStore(const FIoStoreTocResourceInfo& TocResource, const FIoOffsetAndLength& OffsetAndLength, const FIoStoreTocEntryMeta& Meta, const FString& InCasPath, const FString& InKey, FAES::FAESKey& OutAESKey)
{
	FAES::FAESKey AESKey;

	TArray<uint8> DecodedBuffer;
	if (!FBase64::Decode(InKey, DecodedBuffer))
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("AES encryption key base64[%s] is not base64 format!"), *InKey);
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("AES encryption key[%s] is not base64 format!"), *InKey));
		return false;
	}

	// Error checking
	if (DecodedBuffer.Num() != FAES::FAESKey::KeySize)
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("AES encryption key base64[%s] can not decode to %d bytes long!"), *InKey, FAES::FAESKey::KeySize);
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("AES encryption key base64[%s] can not decode to %d bytes long!"), *InKey, FAES::FAESKey::KeySize));
		return false;
	}

	FMemory::Memcpy(AESKey.Key, DecodedBuffer.GetData(), FAES::FAESKey::KeySize);

	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	TUniquePtr<IAsyncReadFileHandle> CasFileHandle(PlatformFile.OpenAsyncRead(*InCasPath));
	if (!CasFileHandle.IsValid())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Preload toc file failed! Open ucas failed! Path: %s."), *InCasPath);
		return false;
	}

	TArray<uint8> CompressedBuffer;
	TArray<uint8> UncompressedBuffer;

	const uint64 CompressionBlockSize = TocResource.Header.CompressionBlockSize;

	TArray<uint8> RawData;
	RawData.AddZeroed(OffsetAndLength.GetLength());
	int32 FirstBlockIndex = int32(OffsetAndLength.GetOffset() / CompressionBlockSize);
	int32 LastBlockIndex = int32((Align(OffsetAndLength.GetOffset() + OffsetAndLength.GetLength(), CompressionBlockSize) - 1) / CompressionBlockSize);
	uint64 OffsetInBlock = OffsetAndLength.GetOffset() % CompressionBlockSize;
	uint8* Dst = RawData.GetData();
	uint8* Src = nullptr;
	uint64 RemainingSize = OffsetAndLength.GetLength();
	for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
	{
		const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];
		uint32 RawSize = Align(CompressionBlock.GetCompressedSize(), FAES::AESBlockSize);
		if (uint32(CompressedBuffer.Num()) < RawSize)
		{
			CompressedBuffer.SetNumUninitialized(RawSize);
		}
		uint32 UncompressedSize = CompressionBlock.GetUncompressedSize();
		if (uint32(UncompressedBuffer.Num()) < UncompressedSize)
		{
			UncompressedBuffer.SetNumUninitialized(UncompressedSize);
		}

		TUniquePtr<IAsyncReadRequest> ReadRequest(CasFileHandle->ReadRequest(CompressionBlock.GetOffset(), RawSize, AIOP_Normal, nullptr, CompressedBuffer.GetData()));
		{
			ReadRequest->WaitCompletion();
		}

		FAES::DecryptData(CompressedBuffer.GetData(), RawSize, AESKey);

		if (CompressionBlock.GetCompressionMethodIndex() == 0)
		{
			Src = CompressedBuffer.GetData();
		}
		else
		{
			FName CompressionMethod = TocResource.CompressionMethods[CompressionBlock.GetCompressionMethodIndex()];
			bool bUncompressed = FCompression::UncompressMemory(CompressionMethod, UncompressedBuffer.GetData(), UncompressedSize, CompressedBuffer.GetData(), CompressionBlock.GetCompressedSize());
			if (!bUncompressed)
			{
				return false;
			}
			Src = UncompressedBuffer.GetData();
		}
		uint64 SizeInBlock = FMath::Min(CompressionBlockSize - OffsetInBlock, RemainingSize);
		FMemory::Memcpy(Dst, Src + OffsetInBlock, SizeInBlock);
		OffsetInBlock = 0;
		RemainingSize -= SizeInBlock;
		Dst += SizeInBlock;
	}

	FIoHash ChunkHash = FIoHash::HashBuffer(RawData.GetData(), RawData.Num());
	if (ChunkHash != Meta.ChunkHash)
	{
		return false;
	}

	OutAESKey = AESKey;

	return true;
}

bool FIoStoreAnalyzer::FillPackageInfo(const FIoStoreTocResourceInfo& TocResource, FStorePackageInfo& OutPackageInfo)
{
	OutPackageInfo.SerializeSize = 0;
	OutPackageInfo.CompressionBlockSize = TocResource.Header.CompressionBlockSize;
	OutPackageInfo.CompressionBlockCount = 0;

	const int32 FirstBlockIndex = int32(OutPackageInfo.ChunkInfo.Offset / OutPackageInfo.CompressionBlockSize);
	const int32 LastBlockIndex = int32((Align(OutPackageInfo.ChunkInfo.Offset + OutPackageInfo.ChunkInfo.Size, OutPackageInfo.CompressionBlockSize) - 1) / OutPackageInfo.CompressionBlockSize);
	for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
	{
		if (!TocResource.CompressionBlocks.IsValidIndex(BlockIndex))
		{
			return false;
		}

		const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource.CompressionBlocks[BlockIndex];
		OutPackageInfo.SerializeSize += Align(CompressionBlock.GetCompressedSize(), FAES::AESBlockSize);

		if (BlockIndex == FirstBlockIndex)
		{
			OutPackageInfo.CompressionMethod = TocResource.CompressionMethods[CompressionBlock.GetCompressionMethodIndex()];
		}
		
		OutPackageInfo.CompressionBlockCount += 1;
	}

	int32 Index = INDEX_NONE;

	// First, try to find the chunk using the Perfect Hash system
	if (TocResource.ChunkPerfectHashSeeds.Num() > 0)
	{
		int32 Seed = FIoStoreTocResourceInfo::HashChunkIdWithSeed(OutPackageInfo.ChunkId, TocResource.ChunkPerfectHashSeeds);
		Index = TocResource.ChunkPerfectHashSeeds.IsValidIndex(Seed) ? Seed : INDEX_NONE;
	}

	// If the perfect hash lookup fails, fallback to a linear search
	if (Index == INDEX_NONE)
	{
		int32 FallbackIndex = INDEX_NONE;
		for (int32 i = 0; i < TocResource.ChunkIds.Num(); ++i)
		{
			if (TocResource.ChunkIds[i] == OutPackageInfo.ChunkId)
			{
				FallbackIndex = i;
				break;
			}
		}

		if (FallbackIndex != INDEX_NONE)
		{
			Index = FallbackIndex;
		}
	}

	// Check if the index is valid and assign the chunk hash
	if (Index != INDEX_NONE && TocResource.ChunkMetas.IsValidIndex(Index))
	{
		OutPackageInfo.ChunkHash = LexToString(TocResource.ChunkMetas[Index].ChunkHash);
	}

	return true;
}

void FIoStoreAnalyzer::OnExtractFiles()
{
	TAtomic<int32> TotalErrorCount{ 0 };
	TAtomic<int32> TotalCompleteCount{ 0 };
	const int32 TotalTotalCount = PendingExtracePackages.Num();

	ParallelFor(PendingExtracePackages.Num(), [this, TotalTotalCount, &TotalErrorCount, &TotalCompleteCount](int32 Index)
	{
		if (IsStopExtract)
		{
			return;
		}

		const int32 PackageIndex = PendingExtracePackages[Index];
		if (!PackageInfos.IsValidIndex(PackageIndex))
		{
			TotalCompleteCount.IncrementExchange();
			TotalErrorCount.IncrementExchange();
			UpdateExtractProgress(TotalTotalCount, TotalCompleteCount, TotalErrorCount);
			return;
		}

		const FStorePackageInfo& PackageInfo = PackageInfos[PackageIndex];
		TSharedPtr<FIoStoreReader>& Reader = StoreContainers[PackageInfo.ContainerIndex].Reader;
		if (!Reader.IsValid())
		{
			TotalCompleteCount.IncrementExchange();
			TotalErrorCount.IncrementExchange();
			UpdateExtractProgress(TotalTotalCount, TotalCompleteCount, TotalErrorCount);
			return;
		}

		FIoReadOptions ReadOptions;
		TIoStatusOr<FIoBuffer> IoBuffer = Reader->Read(PackageInfo.ChunkId, ReadOptions);
		if (!IoBuffer.IsOk())
		{
			TotalCompleteCount.IncrementExchange();
			TotalErrorCount.IncrementExchange();
			UpdateExtractProgress(TotalTotalCount, TotalCompleteCount, TotalErrorCount);
			return;
		}

		const FString OutputFilePath = ExtractOutputPath / PackageInfo.PackageName.ToString() + TEXT(".") + PackageInfo.Extension.ToString();
		const FString BasePath = FPaths::GetPath(OutputFilePath);
		if (!FPaths::DirectoryExists(BasePath))
		{
			IFileManager::Get().MakeDirectory(*BasePath, true);
		}

		IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
		TUniquePtr<IFileHandle>	FileHandle(PlatformFile.OpenWrite(*OutputFilePath));
		if (!FileHandle.IsValid())
		{
			TotalCompleteCount.IncrementExchange();
			TotalErrorCount.IncrementExchange();
			UpdateExtractProgress(TotalTotalCount, TotalCompleteCount, TotalErrorCount);
			return;
		}

		bool bExtractSuccess = true;
		const uint8* PackageSummaryData = IoBuffer.ValueOrDie().Data();
		uint64 DataSize = IoBuffer.ValueOrDie().DataSize();
		if (PackageInfo.ChunkType == EIoChunkType::ExportBundleData)
		{
			const FZenPackageSummary* PackageSummary = reinterpret_cast<const FZenPackageSummary*>(PackageSummaryData);
			uint64 HeaderDataSize = PackageSummary->HeaderSize;
			check(HeaderDataSize <= DataSize);
			FString DestFileName = FPaths::ChangeExtension(OutputFilePath, TEXT(".uheader"));
			if (!FileHandle->Write(PackageSummaryData, HeaderDataSize))
			{
				bExtractSuccess = false;
			}
			DestFileName = FPaths::ChangeExtension(OutputFilePath, TEXT(".uexp"));
			if (!FileHandle->Write(PackageSummaryData + HeaderDataSize, DataSize - HeaderDataSize))
			{
				bExtractSuccess = false;
			}
		}
		else
		{
			if (!FileHandle->Write(PackageSummaryData, DataSize))
			{
				bExtractSuccess = false;
			}
		}

		FileHandle->Flush();

		if (bExtractSuccess)
		{
			TotalCompleteCount.IncrementExchange();
		}
		else
		{
			TotalErrorCount.IncrementExchange();
		}

		UpdateExtractProgress(TotalTotalCount, TotalCompleteCount, TotalErrorCount);
	}, EParallelForFlags::Unbalanced);
}

void FIoStoreAnalyzer::StopExtract()
{
	IsStopExtract.AtomicSet(true);

	for (auto& Thread : ExtractThread)
	{
		Thread.Wait();
	}

	ExtractThread.Empty();
	IsStopExtract.AtomicSet(false);
	PendingExtracePackages.Empty();
}

void FIoStoreAnalyzer::UpdateExtractProgress(int32 InTotal, int32 InComplete, int32 InError)
{
	FFunctionGraphTask::CreateAndDispatchWhenReady(
		[InTotal, InComplete, InError]()
		{
			FPakAnalyzerDelegates::OnUpdateExtractProgress.ExecuteIfBound(InComplete, InError, InTotal);
		},
		TStatId(), nullptr, ENamedThreads::GameThread);
}

void FIoStoreAnalyzer::ParseChunkInfo(const FIoChunkId& InChunkId, FPackageId& OutPackageId, EIoChunkType& OutChunkType)
{
	const uint8* Data = (const uint8*)(&InChunkId);

	*(uint64*)(&OutPackageId) = *(uint64*)(&Data[0]);
	OutChunkType = (EIoChunkType)(*(uint8*)(&Data[11]));
}

FName FIoStoreAnalyzer::FindObjectName(FPackageObjectIndex Index, const FStorePackageInfo* PackageInfo)
{
	if (Index.IsNull())
	{
		return NAME_None;
	}
	else if (Index.IsPackageImport())
	{
		const FIoStoreExport* Export = ExportByGlobalIdMap.FindRef(Index);
		if (Export)
		{
			return Export->FullName;
		}
		else
		{
			return TEXT("Missing package import!");
		}
	}
	else if (Index.IsScriptImport())
	{
		const FScriptObjectDesc* ScriptObjectDesc = ScriptObjectByGlobalIdMap.Find(Index);
		if (ScriptObjectDesc)
		{
			return ScriptObjectDesc->FullName;
		}
		else
		{
			return TEXT("Missing script import!");
		}
	}
	else
	{
		if (PackageInfo)
		{
			if (PackageInfo->Exports.IsValidIndex(Index.Value()))
			{
				return PackageInfo->Exports[Index.Value()].FullName;
			}
			else
			{
				return TEXT("Missing export!");
			}
		}
		else
		{
			return NAME_None;
		}
	}
}

#endif // ENABLE_IO_STORE_ANALYZER
