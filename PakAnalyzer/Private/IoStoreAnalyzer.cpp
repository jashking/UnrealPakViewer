#include "IoStoreAnalyzer.h"

#if ENABLE_IO_STORE_ANALYZER

#include "Algo/Sort.h"
#include "Async/AsyncFileHandle.h"
#include "Async/ParallelFor.h"
#include "Containers/ArrayView.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "Misc/Base64.h"
#include "Misc/Compression.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/AsyncLoading2.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/MemoryReader.h"
#include "UObject/NameBatchSerialization.h"
#include "UObject/ObjectVersion.h"

#if ENGINE_MAJOR_VERSION >= 5
#include "Serialization/PackageStore.h"
#endif // ENGINE_MAJOR_VERSION >= 5

#include "CommonDefines.h"

static TMap<FPackageObjectIndex, FScriptObjectDesc> ScriptObjectByGlobalIdMap;
static TMap<uint64, FIoStoreTocResourceInfo> TocResources;

FIoStoreAnalyzer::FIoStoreAnalyzer()
{

}

FIoStoreAnalyzer::~FIoStoreAnalyzer()
{
	StopExtract();
	Reset();
}

bool FIoStoreAnalyzer::LoadPakFile(const FString& InPakPath, const FString& InAESKey)
{
	if (InPakPath.IsEmpty())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load iostore file failed! Path is empty!"));
		return false;
	}

	UE_LOG(LogPakAnalyzer, Log, TEXT("Start load iostore file: %s."), *InPakPath);

	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();
	if (!PlatformFile.FileExists(*InPakPath))
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Load iostore file failed! File not exists! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Load iostore file failed! File not exists! Path: %s."), *InPakPath);
		return false;
	}

	Reset();

	DefaultAESKey = InAESKey;

	// Make tree root
	TreeRoot = MakeShared<FPakTreeEntry>(FPaths::GetCleanFilename(InPakPath), TEXT(""), true);

	if (!InitializeGlobalReader(InPakPath))
	{
		UE_LOG(LogPakAnalyzer, Warning, TEXT("Load iostore global container failed!"));
	}

	TArray<FString> IoStorePaks = { InPakPath };
	if (!InitializeReaders(IoStorePaks))
	{
		FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("Read iostore file failed! Path: %s."), *InPakPath));
		UE_LOG(LogPakAnalyzer, Error, TEXT("Read iostore file failed! Path: %s."), *InPakPath);
		return false;
	}

	{
		FScopeLock Lock(&CriticalSection);

		for (int32 i = 0; i < PackageInfos.Num(); ++i)
		{
			const FStorePackageInfo& Package = PackageInfos[i];
			FPakEntry Entry;
			Entry.Offset = Package.ChunkInfo.Offset;
			Entry.UncompressedSize = Package.ChunkInfo.Size;
			Entry.Size = Package.SerializeSize;
			Entry.CompressionBlockSize = Package.CompressionBlockSize;
			Entry.CompressionBlocks.AddZeroed(Package.CompressionBlockCount);
			HexToBytes(Package.ChunkHash, Entry.Hash);
			Entry.SetEncrypted(StoreContainers[Package.ContainerIndex].bEncrypted);

			const FString FullPath = Package.PackageName.ToString() + TEXT(".") + Package.Extension.ToString();
			FPakTreeEntryPtr ResultEntry = InsertFileToTree(FullPath, Entry);
			if (ResultEntry.IsValid())
			{
				ResultEntry->CompressionMethod = Package.CompressionMethod;
			}

			FileToPackageIndex.Add(FullPath, i);
		}
	}

	// Generate unique id
	LoadGuid = FGuid::NewGuid();

	RefreshTreeNode(TreeRoot);
	RefreshTreeNodeSizePercent(TreeRoot);
	RefreshClassMap(TreeRoot);

	bHasPakLoaded = true;

	UE_LOG(LogPakAnalyzer, Log, TEXT("Finish load iostore file: %s."), *InPakPath);

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

const FPakFileSumary& FIoStoreAnalyzer::GetPakFileSumary() const
{
	return StoreContainers.Num() > 0 ? StoreContainers[0].Summary : PakFileSumary;
}

void FIoStoreAnalyzer::Reset()
{
	FBaseAnalyzer::Reset();

	GlobalIoStoreReader.Reset();
	GlobalNameMap.Empty();

	ScriptObjectByGlobalIdMap.Empty();
	TocResources.Empty();

	for (FContainerInfo& Container : StoreContainers)
	{
		Container.Reader.Reset();
	}

	StoreContainers.Empty();
	PackageInfos.Empty();
	FileToPackageIndex.Empty();

	DefaultAESKey = TEXT("");
}

TSharedPtr<FIoStoreReader> FIoStoreAnalyzer::CreateIoStoreReader(const FString& InPath)
{
#if ENGINE_MAJOR_VERSION <= 4
	FIoStoreEnvironment IoEnvironment;
	IoEnvironment.InitializeFileEnvironment(FPaths::SetExtension(InPath, TEXT("")));
#endif

	TMap<FGuid, FAES::FAESKey> DecryptionKeys;
	if (!PreLoadIoStore(FPaths::SetExtension(InPath, TEXT("utoc")), FPaths::SetExtension(InPath, TEXT("ucas")), DecryptionKeys))
	{
		return nullptr;
	}

	TSharedPtr<FIoStoreReader> Reader = MakeShared<FIoStoreReader>();

#if ENGINE_MAJOR_VERSION <= 4
	FIoStatus Status = Reader->Initialize(IoEnvironment, DecryptionKeys);
#else
	FIoStatus Status = Reader->Initialize(*FPaths::SetExtension(InPath, TEXT("")), DecryptionKeys);
#endif
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
	const FString GlobalContainerPath = FPaths::GetPath(InPakPath) / TEXT("global");
	GlobalIoStoreReader = CreateIoStoreReader(GlobalContainerPath);
	if (!GlobalIoStoreReader.IsValid())
	{
		UE_LOG(LogPakAnalyzer, Error, TEXT("IoStore failed load global container: %s!"), *GlobalContainerPath);
		return false;
	}

	UE_LOG(LogPakAnalyzer, Display, TEXT("IoStore loading global name map..."));

	TIoStatusOr<FIoBuffer> GlobalNamesIoBuffer = GlobalIoStoreReader->Read(CreateIoChunkId(0, 0, EIoChunkType::LoaderGlobalNames), FIoReadOptions());
	if (!GlobalNamesIoBuffer.IsOk())
	{
		UE_LOG(LogPakAnalyzer, Warning, TEXT("IoStore failed reading names chunk from global container '%s'"), *GlobalContainerPath);
		return false;
	}

	TIoStatusOr<FIoBuffer> GlobalNameHashesIoBuffer = GlobalIoStoreReader->Read(CreateIoChunkId(0, 0, EIoChunkType::LoaderGlobalNameHashes), FIoReadOptions());
	if (!GlobalNameHashesIoBuffer.IsOk())
	{
		UE_LOG(LogPakAnalyzer, Warning, TEXT("IoStore failed reading name hashes chunk from global container '%s'"), *GlobalContainerPath);
		return false;
	}

	LoadNameBatch(
		GlobalNameMap,
		TArrayView<const uint8>(GlobalNamesIoBuffer.ValueOrDie().Data(), GlobalNamesIoBuffer.ValueOrDie().DataSize()),
		TArrayView<const uint8>(GlobalNameHashesIoBuffer.ValueOrDie().Data(), GlobalNameHashesIoBuffer.ValueOrDie().DataSize()));

	UE_LOG(LogPakAnalyzer, Display, TEXT("IoStore loading script imports..."));

	TIoStatusOr<FIoBuffer> InitialLoadIoBuffer = GlobalIoStoreReader->Read(CreateIoChunkId(0, 0, EIoChunkType::LoaderInitialLoadMeta), FIoReadOptions());
	if (!InitialLoadIoBuffer.IsOk())
	{
		UE_LOG(LogPakAnalyzer, Warning, TEXT("IoStore failed reading initial load meta chunk from global container '%s'"), *GlobalContainerPath);
		return false;
	}

	FLargeMemoryReader InitialLoadArchive(InitialLoadIoBuffer.ValueOrDie().Data(), InitialLoadIoBuffer.ValueOrDie().DataSize());
	int32 NumScriptObjects = 0;
	InitialLoadArchive << NumScriptObjects;
	const FScriptObjectEntry* ScriptObjectEntries = reinterpret_cast<const FScriptObjectEntry*>(InitialLoadIoBuffer.ValueOrDie().Data() + InitialLoadArchive.Tell());
	for (int32 ScriptObjectIndex = 0; ScriptObjectIndex < NumScriptObjects; ++ScriptObjectIndex)
	{
		const FScriptObjectEntry& ScriptObjectEntry = ScriptObjectEntries[ScriptObjectIndex];
		const FMappedName& MappedName = FMappedName::FromMinimalName(ScriptObjectEntry.ObjectName);
		check(MappedName.IsGlobal());
		FScriptObjectDesc& ScriptObjectDesc = ScriptObjectByGlobalIdMap.Add(ScriptObjectEntry.GlobalIndex);
		ScriptObjectDesc.Name = FName::CreateFromDisplayId(GlobalNameMap[MappedName.GetIndex()], MappedName.GetNumber());
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

	return true;
}

bool FIoStoreAnalyzer::InitializeReaders(const TArray<FString>& InPaks)
{
	static const EParallelForFlags ParallelForFlags = EParallelForFlags::Unbalanced /*| EParallelForFlags::ForceSingleThread*/;
	FCriticalSection Mutex;

	for (const FString& ContainerFilePath : InPaks)
	{
		TSharedPtr<FIoStoreReader> Reader = CreateIoStoreReader(ContainerFilePath);
		if (!Reader.IsValid())
		{
			UE_LOG(LogPakAnalyzer, Warning, TEXT("Failed to read container '%s'"), *ContainerFilePath);
			continue;
		}

		FContainerInfo Info;
		Info.Reader = MoveTemp(Reader);
		Info.Summary.PakFilePath = ContainerFilePath;

		StoreContainers.Add(Info);
	}

	ParallelFor(StoreContainers.Num(), [this, &Mutex](int32 Index)
	{
		FContainerInfo& ContainerInfo = StoreContainers[Index];

		ContainerInfo.Id = ContainerInfo.Reader->GetContainerId();
		ContainerInfo.EncryptionKeyGuid = ContainerInfo.Reader->GetEncryptionKeyGuid();
		ContainerInfo.Summary.MountPoint = ContainerInfo.Reader->GetDirectoryIndexReader().GetMountPoint();
		EIoContainerFlags Flags = ContainerInfo.Reader->GetContainerFlags();
		ContainerInfo.bCompressed = bool(Flags & EIoContainerFlags::Compressed);
		ContainerInfo.bEncrypted = bool(Flags & EIoContainerFlags::Encrypted);
		ContainerInfo.bSigned = bool(Flags & EIoContainerFlags::Signed);
		ContainerInfo.bIndexed = bool(Flags & EIoContainerFlags::Indexed);
		ContainerInfo.Summary.PakInfo.bEncryptedIndex = ContainerInfo.bEncrypted;
		ContainerInfo.Summary.PakInfo.EncryptionKeyGuid = ContainerInfo.EncryptionKeyGuid;

		FIoStoreTocResourceInfo* TocResource = TocResources.Find(ContainerInfo.Id.Value());
		if (TocResource)
		{
			TArray<FString> CompressionMethods;
			for (FName CompressionName : TocResource->CompressionMethods)
			{
				CompressionMethods.Add(CompressionName.ToString());
			}
			ContainerInfo.Summary.CompressionMethods = FString::Join(CompressionMethods, TEXT(", "));

			const int64 CasFileSize = IPlatformFile::GetPlatformPhysical().FileSize(*ContainerInfo.Summary.PakFilePath);
			ContainerInfo.Summary.PakFileSize = CasFileSize + TocResource->TocFileSize;
			ContainerInfo.Summary.PakInfo.IndexOffset = CasFileSize;
		}

		TIoStatusOr<FIoBuffer> IoBuffer = ContainerInfo.Reader->Read(CreateIoChunkId(ContainerInfo.Reader->GetContainerId().Value(), 0, EIoChunkType::ContainerHeader), FIoReadOptions());
		if (IoBuffer.IsOk())
		{
			FMemoryReaderView Ar(MakeArrayView(IoBuffer.ValueOrDie().Data(), IoBuffer.ValueOrDie().DataSize()));
			FContainerHeader ContainerHeader;
			Ar << ContainerHeader;

			TArrayView<FPackageStoreEntry> StoreEntries(reinterpret_cast<FPackageStoreEntry*>(ContainerHeader.StoreEntries.GetData()), ContainerHeader.PackageCount);

			int32 PackageIndex = 0;
			for (FPackageStoreEntry& ContainerEntry : StoreEntries)
			{
				FStorePackageInfo PackageInfo;
				PackageInfo.PackageId = ContainerHeader.PackageIds[PackageIndex++];
				PackageInfo.ContainerIndex = Index;

				FScopeLock ScopeLock(&Mutex);
				PackageInfos.Add(PackageInfo);
			}
		}
	}, ParallelForFlags);

	TArray<FStorePackageInfo> BulkPackageInfos;
	ParallelFor(PackageInfos.Num(), [this, &BulkPackageInfos, &Mutex](int32 Index)
	{
		FStorePackageInfo& PackageInfo = PackageInfos[Index];
		TSharedPtr<FIoStoreReader>& Reader = StoreContainers[PackageInfo.ContainerIndex].Reader;
		const uint64 ContainerId = StoreContainers[PackageInfo.ContainerIndex].Id.Value();

		PackageInfo.ChunkId = CreateIoChunkId(PackageInfo.PackageId.Value(), 0, EIoChunkType::ExportBundleData);

		FIoReadOptions ReadOptions;

		TIoStatusOr<FIoBuffer> IoBuffer = Reader->Read(PackageInfo.ChunkId, ReadOptions);
		if (!IoBuffer.IsOk())
		{
			return;
		}

		const uint8* PackageSummaryData = IoBuffer.ValueOrDie().Data();
		const FPackageSummary* PackageSummary = reinterpret_cast<const FPackageSummary*>(PackageSummaryData);
		const uint64 PackageSummarySize = PackageSummary->GraphDataOffset + PackageSummary->GraphDataSize;
		if (PackageSummarySize > IoBuffer.ValueOrDie().DataSize())
		{
			ReadOptions.SetRange(0, PackageSummarySize);
			IoBuffer = Reader->Read(PackageInfo.ChunkId, ReadOptions);
			PackageSummaryData = IoBuffer.ValueOrDie().Data();
			PackageSummary = reinterpret_cast<const FPackageSummary*>(PackageSummaryData);
		}

		TArray<FNameEntryId> PackageNameMap;
		if (PackageSummary->NameMapNamesSize)
		{
			const uint8* NameMapNamesData = PackageSummaryData + PackageSummary->NameMapNamesOffset;
			const uint8* NameMapHashesData = PackageSummaryData + PackageSummary->NameMapHashesOffset;
			LoadNameBatch(
				PackageNameMap,
				TArrayView<const uint8>(NameMapNamesData, PackageSummary->NameMapNamesSize),
				TArrayView<const uint8>(NameMapHashesData, PackageSummary->NameMapHashesSize));
		}

		PackageInfo.Extension = TEXT("uexp");
		PackageInfo.ChunkType = EIoChunkType::ExportBundleData;
		PackageInfo.PackageName = FName::CreateFromDisplayId(PackageNameMap[PackageSummary->Name.GetIndex()], PackageSummary->Name.GetNumber());
		TIoStatusOr<FIoStoreTocChunkInfo> ChunkInfo = Reader->GetChunkInfo(PackageInfo.ChunkId);
		if (ChunkInfo.IsOk())
		{
			PackageInfo.ChunkInfo = ChunkInfo.ValueOrDie();
			FillPackageInfo(ContainerId, PackageInfo);
		}

		// Bulk
		TArray<EIoChunkType> BulkTypes = { EIoChunkType::BulkData, EIoChunkType::OptionalBulkData, EIoChunkType::MemoryMappedBulkData };
		for (EIoChunkType Type : BulkTypes)
		{
			FIoChunkId BulkChunkId = CreateIoChunkId(PackageInfo.PackageId.Value(), 0, Type);
			TIoStatusOr<FIoStoreTocChunkInfo> BulkChunkInfo = Reader->GetChunkInfo(BulkChunkId);
			if (BulkChunkInfo.IsOk())
			{
				FStorePackageInfo BulkPackageInfo;
				BulkPackageInfo.ChunkId = BulkChunkId;
				BulkPackageInfo.PackageId = PackageInfo.PackageId;
				BulkPackageInfo.PackageName = PackageInfo.PackageName;
				BulkPackageInfo.ContainerIndex = PackageInfo.ContainerIndex;
				BulkPackageInfo.ChunkType = Type;
				BulkPackageInfo.ChunkInfo = BulkChunkInfo.ValueOrDie();

				switch (Type)
				{
				case EIoChunkType::BulkData: BulkPackageInfo.Extension = TEXT("ubulk"); break;
				case EIoChunkType::OptionalBulkData: BulkPackageInfo.Extension = TEXT("uptnl"); break;
				case EIoChunkType::MemoryMappedBulkData: BulkPackageInfo.Extension = TEXT("umappedbulk"); break;
				default: BulkPackageInfo.Extension = TEXT("");
				}

				FillPackageInfo(ContainerId, BulkPackageInfo);

				FScopeLock ScopeLock(&Mutex);
				BulkPackageInfos.Add(BulkPackageInfo);
			}
		}
	}, ParallelForFlags);

	PackageInfos += BulkPackageInfos;

	return true;
}

bool FIoStoreAnalyzer::PreLoadIoStore(const FString& InTocPath, const FString& InCasPath, TMap<FGuid, FAES::FAESKey>& OutKeys)
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
	TArray<FIoChunkId> ChunkIdsArray;
	const FIoChunkId* ChunkIds = reinterpret_cast<const FIoChunkId*>(TocBuffer.Get());
	ChunkIdsArray = MakeArrayView<FIoChunkId const>(ChunkIds, Header.TocEntryCount);
	if (ChunkIdsArray.Num() <= 0)
	{
		return false;
	}

	for (int32 ChunkIndex = 0; ChunkIndex < ChunkIdsArray.Num(); ++ChunkIndex)
	{
		TocResource.ChunkIdToIndex.Add(ChunkIdsArray[ChunkIndex], ChunkIndex);
	}

	// Chunk offsets
	TArray<FIoOffsetAndLength> ChunkOffsetLengthsArray;
	const FIoOffsetAndLength* ChunkOffsetLengths = reinterpret_cast<const FIoOffsetAndLength*>(ChunkIds + Header.TocEntryCount);
	ChunkOffsetLengthsArray = MakeArrayView<FIoOffsetAndLength const>(ChunkOffsetLengths, Header.TocEntryCount);

	// Compression blocks
	const FIoStoreTocCompressedBlockEntry* CompressionBlocks = reinterpret_cast<const FIoStoreTocCompressedBlockEntry*>(ChunkOffsetLengths + Header.TocEntryCount);
	TocResource.CompressionBlocks = MakeArrayView<FIoStoreTocCompressedBlockEntry const>(CompressionBlocks, Header.TocCompressedBlockEntryCount);

	// Compression methods
	TocResource.CompressionMethods.Reserve(Header.CompressionMethodNameCount + 1);
	TocResource.CompressionMethods.Add(NAME_None);

	const ANSICHAR* AnsiCompressionMethodNames = reinterpret_cast<const ANSICHAR*>(CompressionBlocks + Header.TocCompressedBlockEntryCount);
	for (uint32 CompressonNameIndex = 0; CompressonNameIndex < Header.CompressionMethodNameCount; CompressonNameIndex++)
	{
		const ANSICHAR* AnsiCompressionMethodName = AnsiCompressionMethodNames + CompressonNameIndex * Header.CompressionMethodNameLength;
		TocResource.CompressionMethods.Add(FName(AnsiCompressionMethodName));
	}

	// Chunk block signatures
	const uint8* SignatureBuffer = reinterpret_cast<const uint8*>(AnsiCompressionMethodNames + Header.CompressionMethodNameCount * Header.CompressionMethodNameLength);
	const uint8* DirectoryIndexBuffer = SignatureBuffer;

	const bool bIsSigned = EnumHasAnyFlags(Header.ContainerFlags, EIoContainerFlags::Signed);
	if (bIsSigned)
	{
		const int32* HashSize = reinterpret_cast<const int32*>(SignatureBuffer);
		TArrayView<const uint8> TocSignature = MakeArrayView<const uint8>(reinterpret_cast<const uint8*>(HashSize + 1), *HashSize);
		TArrayView<const uint8> BlockSignature = MakeArrayView<const uint8>(TocSignature.GetData() + *HashSize, *HashSize);
		TArrayView<const FSHAHash> ChunkBlockSignatures = MakeArrayView<const FSHAHash>(reinterpret_cast<const FSHAHash*>(BlockSignature.GetData() + *HashSize), Header.TocCompressedBlockEntryCount);

		// Adjust address to meta data
		DirectoryIndexBuffer = reinterpret_cast<const uint8*>(ChunkBlockSignatures.GetData() + ChunkBlockSignatures.Num());
	}

	// Meta
	const uint8* TocMeta = DirectoryIndexBuffer + Header.DirectoryIndexSize;
	const FIoStoreTocEntryMeta* ChunkMetas = reinterpret_cast<const FIoStoreTocEntryMeta*>(TocMeta);
	TocResource.ChunkMetas = MakeArrayView<FIoStoreTocEntryMeta const>(ChunkMetas, Header.TocEntryCount);

	bool bShouldLoad = true;
	if (Header.EncryptionKeyGuid.IsValid() || EnumHasAnyFlags(Header.ContainerFlags, EIoContainerFlags::Encrypted))
	{
		FAES::FAESKey AESKey;

		bShouldLoad = DefaultAESKey.IsEmpty() ? false : TryDecryptIoStore(TocResource, ChunkOffsetLengthsArray[0], TocResource.ChunkMetas[0], InCasPath, DefaultAESKey, AESKey);

		if (!bShouldLoad)
		{
			if (FPakAnalyzerDelegates::OnGetAESKey.IsBound())
			{
				bool bCancel = true;
				do
				{
					const FString KeyString = FPakAnalyzerDelegates::OnGetAESKey.Execute(bCancel);

					bShouldLoad = !bCancel ? TryDecryptIoStore(TocResource, ChunkOffsetLengthsArray[0], TocResource.ChunkMetas[0], InCasPath, KeyString, AESKey) : false;
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
		CachedAESKey = AESKey;
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

	FIoChunkHash ChunkHash = FIoChunkHash::HashBuffer(RawData.GetData(), RawData.Num());
	if (ChunkHash != Meta.ChunkHash)
	{
		return false;
	}

	OutAESKey = AESKey;

	return true;
}

bool FIoStoreAnalyzer::FillPackageInfo(uint64 ContainerId, FStorePackageInfo& OutPackageInfo)
{
	FIoStoreTocResourceInfo* TocResource = TocResources.Find(ContainerId);
	if (!TocResource)
	{
		return false;
	}

	OutPackageInfo.SerializeSize = 0;
	OutPackageInfo.CompressionBlockSize = TocResource->Header.CompressionBlockSize;
	OutPackageInfo.CompressionBlockCount = 0;

	const int32 FirstBlockIndex = int32(OutPackageInfo.ChunkInfo.Offset / OutPackageInfo.CompressionBlockSize);
	const int32 LastBlockIndex = int32((Align(OutPackageInfo.ChunkInfo.Offset + OutPackageInfo.ChunkInfo.Size, OutPackageInfo.CompressionBlockSize) - 1) / OutPackageInfo.CompressionBlockSize);
	for (int32 BlockIndex = FirstBlockIndex; BlockIndex <= LastBlockIndex; ++BlockIndex)
	{
		if (!TocResource->CompressionBlocks.IsValidIndex(BlockIndex))
		{
			return false;
		}

		const FIoStoreTocCompressedBlockEntry& CompressionBlock = TocResource->CompressionBlocks[BlockIndex];
		OutPackageInfo.SerializeSize += Align(CompressionBlock.GetCompressedSize(), FAES::AESBlockSize);

		if (BlockIndex == FirstBlockIndex)
		{
			OutPackageInfo.CompressionMethod = TocResource->CompressionMethods[CompressionBlock.GetCompressionMethodIndex()];
		}
		
		OutPackageInfo.CompressionBlockCount += 1;
	}

	int32* Index = TocResource->ChunkIdToIndex.Find(OutPackageInfo.ChunkId);
	if (Index && TocResource->ChunkMetas.IsValidIndex(*Index))
	{
		OutPackageInfo.ChunkHash = TocResource->ChunkMetas[*Index].ChunkHash.ToString();
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

		if (PackageInfo.ChunkType == EIoChunkType::ExportBundleData)
		{
			const uint8* PackageSummaryData = IoBuffer.ValueOrDie().Data();
			const FPackageSummary* PackageSummary = reinterpret_cast<const FPackageSummary*>(PackageSummaryData);

			const uint8* Start = PackageSummaryData + PackageSummary->GraphDataOffset + PackageSummary->GraphDataSize;
			const uint8* End = PackageSummaryData + IoBuffer.ValueOrDie().DataSize();

			FileHandle->Write(Start, End - Start);

			const uint32 PackageTag = PACKAGE_FILE_TAG;
			FileHandle->Write((const uint8*)&PackageTag, 4);
		}
		else
		{
			FileHandle->Write(IoBuffer.ValueOrDie().Data(), IoBuffer.ValueOrDie().DataSize());
		}

		FileHandle->Flush();

		TotalCompleteCount.IncrementExchange();

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

#endif // ENABLE_IO_STORE_ANALYZER
