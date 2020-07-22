#include "ExtractThreadWorker.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "IPlatformFilePak.h"
#include "Misc/Compression.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"

#include "CommonDefines.h"

FExtractThreadWorker::FExtractThreadWorker()
	: Thread(nullptr)
	, PakVersion(FPakInfo::PakFile_Version_Latest)
{
	Guid = FGuid::NewGuid();
}

FExtractThreadWorker::~FExtractThreadWorker()
{
	Shutdown();
}

bool FExtractThreadWorker::Init()
{
	return true;
}

uint32 FExtractThreadWorker::Run()
{
	FArchive* ReaderArchive = IFileManager::Get().CreateFileReader(*PakFilePath);

	const int64 BufferSize = 8 * 1024 * 1024; // 8MB buffer for extracting
	void* Buffer = FMemory::Malloc(BufferSize);
	uint8* PersistantCompressionBuffer = NULL;
	int64 CompressionBufferSize = 0;

	const bool bHasRelativeCompressedChunkOffsets = PakVersion >= FPakInfo::PakFile_Version_RelativeChunkOffsets;

	int32 CompleteCount = 0;
	int32 ErrorCount = 0;
	const int32 TotalCount = Files.Num();

	for (const FPakFileEntry& File : Files)
	{
		if (StopTaskCounter.GetValue() > 0)
		{
			break;
		}

		ReaderArchive->Seek(File.PakEntry.Offset);

		FPakEntry EntryInfo;
		EntryInfo.Serialize(*ReaderArchive, PakVersion);
		if (File.PakEntry == EntryInfo)
		{
			const FString OutputFilePath = OutputPath / File.Path;
			const FString BasePath = FPaths::GetPath(OutputFilePath);
			if (!FPaths::DirectoryExists(BasePath))
			{
				IFileManager::Get().MakeDirectory(*BasePath, true);
			}

			TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*OutputFilePath));
			if (FileHandle)
			{
				if (EntryInfo.CompressionMethodIndex == 0)
				{
					if (!BufferedCopyFile(*FileHandle, *ReaderArchive, File.PakEntry, Buffer, BufferSize, AESKey))
					{
						// Add to failed list
						++ErrorCount;
						UE_LOG(LogPakAnalyzer, Error, TEXT("Extract none-compressed file failed! File: %s"), *File.Path);
					}
				}
				else
				{
					if (!UncompressCopyFile(*FileHandle, *ReaderArchive, File.PakEntry, PersistantCompressionBuffer, CompressionBufferSize, AESKey, File.CompressionMethod, bHasRelativeCompressedChunkOffsets))
					{
						// Add to failed list
						++ErrorCount;
						UE_LOG(LogPakAnalyzer, Error, TEXT("Extract compressed file failed! File: %s"), *File.Path);
					}
				}
			}
			else
			{
				// open to write failed
				++ErrorCount;
				UE_LOG(LogPakAnalyzer, Error, TEXT("Open local file to write failed! File: %s"), *OutputFilePath);
			}
		}
		else
		{
			// mismatch
			++ErrorCount;
			UE_LOG(LogPakAnalyzer, Error, TEXT("Extract file failed! PakEntry mismatch! File: %s"), *File.Path);
		}

		++CompleteCount;

		OnUpdateExtractProgress.ExecuteIfBound(Guid, CompleteCount, ErrorCount, TotalCount);
		//FPlatformProcess::SleepNoStats(0);
	}

	FMemory::Free(Buffer);
	FMemory::Free(PersistantCompressionBuffer);

	ReaderArchive->Close();
	delete ReaderArchive;

	if (CompleteCount == TotalCount)
	{
		UE_LOG(LogPakAnalyzer, Log, TEXT("Extract worker: %s finished, file count: %d, error count: %d."), *Guid.ToString(), TotalCount, ErrorCount);
	}
	else
	{
		UE_LOG(LogPakAnalyzer, Warning, TEXT("Extract worker: %s interrupted, file count: %d, complete count: %d, error count: %d."), *Guid.ToString(), TotalCount, CompleteCount, ErrorCount);
	}

	StopTaskCounter.Reset();
	return 0;
}

void FExtractThreadWorker::Stop()
{
	StopTaskCounter.Increment();
	EnsureCompletion();
	StopTaskCounter.Reset();
}

void FExtractThreadWorker::Exit()
{

}

void FExtractThreadWorker::Shutdown()
{
	Stop();

	if (Thread)
	{
		UE_LOG(LogPakAnalyzer, Log, TEXT("Shutdown extract worker: %s."), *Guid.ToString());

		delete Thread;
		Thread = nullptr;
	}
}

void FExtractThreadWorker::EnsureCompletion()
{
	if (Thread)
	{
		Thread->WaitForCompletion();
	}
}

void FExtractThreadWorker::StartExtract(const FString& InPakFile, int32 InPakVersion, const FAES::FAESKey& InKey, const FString& InOutputPath)
{
	Shutdown();

	UE_LOG(LogPakAnalyzer, Log, TEXT("Start extract worker: %s, output: %s, file count: %d, pak version: %d."), *Guid.ToString(), *InOutputPath, Files.Num(), InPakVersion);

	PakFilePath = InPakFile;
	OutputPath = InOutputPath;
	PakVersion = InPakVersion;
	AESKey = InKey;

	Thread = FRunnableThread::Create(this, TEXT("ExtractThreadWorker"), 0, EThreadPriority::TPri_Highest);
}

void FExtractThreadWorker::InitTaskFiles(TArray<FPakFileEntry>& InFiles)
{
	Files = MoveTemp(InFiles);
}

FExtractThreadWorker::FOnUpdateExtractProgress& FExtractThreadWorker::GetOnUpdateExtractProgressDelegate()
{
	return OnUpdateExtractProgress;
}

bool FExtractThreadWorker::BufferedCopyFile(FArchive& Dest, FArchive& Source, const FPakEntry& Entry, void* Buffer, int64 BufferSize, const FAES::FAESKey& InKey)
{
	// Align down
	BufferSize = BufferSize & ~(FAES::AESBlockSize - 1);
	int64 RemainingSizeToCopy = Entry.Size;
	while (RemainingSizeToCopy > 0)
	{
		const int64 SizeToCopy = FMath::Min(BufferSize, RemainingSizeToCopy);
		// If file is encrypted so we need to account for padding
		int64 SizeToRead = Entry.IsEncrypted() ? Align(SizeToCopy, FAES::AESBlockSize) : SizeToCopy;

		Source.Serialize(Buffer, SizeToRead);
		if (Entry.IsEncrypted())
		{
			FAES::DecryptData((uint8*)Buffer, SizeToRead, InKey);
		}
		Dest.Serialize(Buffer, SizeToCopy);
		RemainingSizeToCopy -= SizeToRead;
	}
	return true;
}

bool FExtractThreadWorker::UncompressCopyFile(FArchive& Dest, FArchive& Source, const FPakEntry& Entry, uint8*& PersistentBuffer, int64& BufferSize, const FAES::FAESKey& InKey, FName InCompressionMethod, bool bHasRelativeCompressedChunkOffsets)
{
	if (Entry.UncompressedSize == 0)
	{
		return false;
	}

	// The compression block size depends on the bit window that the PAK file was originally created with. Since this isn't stored in the PAK file itself,
	// we can use FCompression::CompressMemoryBound as a guideline for the max expected size to avoid unncessary reallocations, but we need to make sure
	// that we check if the actual size is not actually greater (eg. UE-59278).
	int32 MaxCompressionBlockSize = FCompression::CompressMemoryBound(InCompressionMethod, Entry.CompressionBlockSize);
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
		uint32 UncompressedBlockSize = (uint32)FMath::Min<int64>(Entry.UncompressedSize - Entry.CompressionBlockSize * BlockIndex, Entry.CompressionBlockSize);
		Source.Seek(Entry.CompressionBlocks[BlockIndex].CompressedStart + (bHasRelativeCompressedChunkOffsets ? Entry.Offset : 0));
		uint32 SizeToRead = Entry.IsEncrypted() ? Align(CompressedBlockSize, FAES::AESBlockSize) : CompressedBlockSize;
		Source.Serialize(PersistentBuffer, SizeToRead);

		if (Entry.IsEncrypted())
		{
			FAES::DecryptData(PersistentBuffer, SizeToRead, InKey);
		}

		if (!FCompression::UncompressMemory(InCompressionMethod, UncompressedBuffer, UncompressedBlockSize, PersistentBuffer, CompressedBlockSize))
		{
			return false;
		}

		Dest.Serialize(UncompressedBuffer, UncompressedBlockSize);
	}

	return true;
}
