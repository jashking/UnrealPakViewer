// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ICompressionFormat.h"
#include "Modules/ModuleManager.h"

/**
 * The public interface to this module
 */
class IOodleModule : public ICompressionFormat
{
public:
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IOodleModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IOodleModule>("Oodle");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("Oodle");
	}

	// Begin ICompressionFormat Interfaces
	virtual FName GetCompressionFormatName() override
	{
		return "Oodle";
	}

	virtual bool Compress(void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize, int32 CompressionData) override
	{
		return false;
	}

	virtual bool Uncompress(void* UncompressedBuffer, int32& UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, int32 CompressionData) override
	{
		return false;
	}

	virtual int32 GetCompressedBufferSize(int32 UncompressedSize, int32 CompressionData) override
	{
		return 0;
	}

	virtual uint32 GetVersion() override
	{
		return 0;
	}

	virtual FString GetDDCKeySuffix() override
	{
		return TEXT("");
	}
	// End ICompressionFormat Interfaces
};
