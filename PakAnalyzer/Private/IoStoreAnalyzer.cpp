#include "IoStoreAnalyzer.h"

#if ENABLE_IO_STORE_ANALYZER

#include "Algo/Sort.h"
#include "Async/ParallelFor.h"
#include "Containers/ArrayView.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformFile.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/AsyncLoading2.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/MemoryReader.h"
#include "UObject/NameBatchSerialization.h"

#include "CommonDefines.h"

struct FIoStoreTocHeader
{
	static constexpr char TocMagicImg[] = "-==--==--==--==-";

	uint8	TocMagic[16];
	uint8	Version;
	uint32	TocHeaderSize;
	uint32	TocEntryCount;
	uint32	TocCompressedBlockEntryCount;
	uint32	TocCompressedBlockEntrySize;	// For sanity checking
	uint32	CompressionMethodNameCount;
	uint32	CompressionMethodNameLength;
	uint32	CompressionBlockSize;
	uint32	DirectoryIndexSize;
	FIoContainerId ContainerId;
	FGuid	EncryptionKeyGuid;
	EIoContainerFlags ContainerFlags;
	uint8	Pad[60];

	void MakeMagic()
	{
		FMemory::Memcpy(TocMagic, TocMagicImg, sizeof TocMagic);
	}

	bool CheckMagic() const
	{
		return FMemory::Memcmp(TocMagic, TocMagicImg, sizeof TocMagic) == 0;
	}
};

struct FScriptObjectDesc
{
	FName Name;
	FName FullName;
	FPackageObjectIndex GlobalImportIndex;
	FPackageObjectIndex OuterIndex;
};

static TMap<FPackageObjectIndex, FScriptObjectDesc> ScriptObjectByGlobalIdMap;

FIoStoreAnalyzer::FIoStoreAnalyzer()
{

}

FIoStoreAnalyzer::~FIoStoreAnalyzer()
{

}

bool FIoStoreAnalyzer::LoadPakFile(const FString& InPakPath)
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

		for (const FStorePackageInfo& Package : PackageInfos)
		{
			FPakEntry Entry;
			Entry.Offset = Package.ChunkInfo.Offset;
			Entry.UncompressedSize = Package.ChunkInfo.Size;

			FString Extension;
			switch (Package.ChunkType)
			{
			case EIoChunkType::ExportBundleData: Extension = TEXT(".uexp"); break;
			case EIoChunkType::BulkData: Extension = TEXT(".ubulk"); break;
			case EIoChunkType::OptionalBulkData: Extension = TEXT(".uptnl"); break;
			case EIoChunkType::MemoryMappedBulkData: Extension = TEXT(".umappedbulk"); break;
			default: Extension = TEXT("");
			}

			InsertFileToTree(Package.PackageName.ToString() + Extension, Entry);
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

}

void FIoStoreAnalyzer::CancelExtract()
{

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

	StoreContainers.Empty();
	PackageInfos.Empty();
}

TSharedPtr<FIoStoreReader> FIoStoreAnalyzer::CreateIoStoreReader(const FString& InPath)
{
	FIoStoreEnvironment IoEnvironment;
	IoEnvironment.InitializeFileEnvironment(FPaths::SetExtension(InPath, TEXT("")));

	TMap<FGuid, FAES::FAESKey> DecryptionKeys;
	if (!PreLoadIoStore(FPaths::SetExtension(InPath, TEXT("utoc")), DecryptionKeys))
	{
		return nullptr;
	}

	TSharedPtr<FIoStoreReader> Reader = MakeShared<FIoStoreReader>();

	FIoStatus Status = Reader->Initialize(IoEnvironment, DecryptionKeys);
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
	static const EParallelForFlags ParallelForFlags = EParallelForFlags::Unbalanced | EParallelForFlags::ForceSingleThread;
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
		Info.Reader = Reader;
		Info.Summary.PakFilePath = ContainerFilePath;
		Info.Summary.PakFileSize = IPlatformFile::GetPlatformPhysical().FileSize(*ContainerFilePath);
		Info.Summary.PakInfo.IndexOffset = Info.Summary.PakFileSize;

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
		TSharedPtr<FIoStoreReader> Reader = StoreContainers[PackageInfo.ContainerIndex].Reader;

		PackageInfo.ChunkId = CreateIoChunkId(PackageInfo.PackageId.Value(), 0, EIoChunkType::ExportBundleData);

		FIoReadOptions ReadOptions;
		ReadOptions.SetRange(0, 16 << 10); // no include export hashes

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

		PackageInfo.ChunkType = EIoChunkType::ExportBundleData;
		PackageInfo.PackageName = FName::CreateFromDisplayId(PackageNameMap[PackageSummary->Name.GetIndex()], PackageSummary->Name.GetNumber());
		TIoStatusOr<FIoStoreTocChunkInfo> ChunkInfo = Reader->GetChunkInfo(PackageInfo.ChunkId);
		if (ChunkInfo.IsOk())
		{
			PackageInfo.ChunkInfo = ChunkInfo.ValueOrDie();
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

				FScopeLock ScopeLock(&Mutex);
				BulkPackageInfos.Add(BulkPackageInfo);
			}
		}
	}, ParallelForFlags);

	PackageInfos += BulkPackageInfos;

	return true;
}

bool FIoStoreAnalyzer::PreLoadIoStore(const FString& InTocPath, TMap<FGuid, FAES::FAESKey>& OutKeys)
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

	if (Header.EncryptionKeyGuid.IsValid() || EnumHasAnyFlags(Header.ContainerFlags, EIoContainerFlags::Encrypted))
	{
		const FString KeyString = TEXT("zf/sDPuWNj/tpAtoXw0ar1SJtT9MOjxqB8Za1P/r91M=");// FPakAnalyzerDelegates::OnGetAESKey.IsBound() ? FPakAnalyzerDelegates::OnGetAESKey.Execute() : TEXT("");

		FAES::FAESKey AESKey;

		TArray<uint8> DecodedBuffer;
		if (!FBase64::Decode(KeyString, DecodedBuffer))
		{
			UE_LOG(LogPakAnalyzer, Error, TEXT("AES encryption key base64[%s] is not base64 format!"), *KeyString);
			FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("AES encryption key[%s] is not base64 format!"), *KeyString));
			return false;
		}

		// Error checking
		if (DecodedBuffer.Num() != FAES::FAESKey::KeySize)
		{
			UE_LOG(LogPakAnalyzer, Error, TEXT("AES encryption key base64[%s] can not decode to %d bytes long!"), *KeyString, FAES::FAESKey::KeySize);
			FPakAnalyzerDelegates::OnLoadPakFailed.ExecuteIfBound(FString::Printf(TEXT("AES encryption key base64[%s] can not decode to %d bytes long!"), *KeyString, FAES::FAESKey::KeySize));
			return false;
		}

		FMemory::Memcpy(AESKey.Key, DecodedBuffer.GetData(), FAES::FAESKey::KeySize);

		OutKeys.Add(Header.EncryptionKeyGuid, AESKey);
		CachedAESKey = AESKey;
	}

	return true;
}

#endif // ENABLE_IO_STORE_ANALYZER
