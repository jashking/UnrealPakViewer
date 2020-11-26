#include "OodleModule.h"

#include "Features/IModularFeatures.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

typedef size_t(*FnOodleLZ_Decompress)(const void* srcBuf, size_t srcLen, void* dstBuf, size_t dstLen, int64 unk1, int64 unk2, int64 unk3, int64 unk4, int64 unk5, int64 unk6, int64 unk7, int64 unk8, int64 unk9, int64 unk10);
typedef size_t(*FnOodleLZ_Compress)(int algo, unsigned char* in, int insz, unsigned char* out, int max, void* a, void* b, void* c);

class FOodleModule : public IOodleModule
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	// Begin ICompressionFormat Interfaces
	virtual bool Uncompress(void* UncompressedBuffer, int32& UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, int32 CompressionData) override;
	// End ICompressionFormat Interfaces

protected:
	void* OodleDllHandle = nullptr;

	FnOodleLZ_Decompress OodleLZ_Decompress = nullptr;
};

IMPLEMENT_MODULE(FOodleModule, Oodle);

void FOodleModule::StartupModule()
{
#if PLATFORM_WINDOWS
	OodleDllHandle = FPlatformProcess::GetDllHandle(TEXT("oo2core_8_win64.dll"));

	OodleLZ_Decompress = reinterpret_cast<FnOodleLZ_Decompress>(FPlatformProcess::GetDllExport(OodleDllHandle, TEXT("OodleLZ_Decompress")));
#endif //PLATFORM_WINDOWS

	IModularFeatures::Get().RegisterModularFeature(COMPRESSION_FORMAT_FEATURE_NAME, this);
}

void FOodleModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(COMPRESSION_FORMAT_FEATURE_NAME, this);

#if PLATFORM_WINDOWS
	if (OodleDllHandle)
	{
		FPlatformProcess::FreeDllHandle(OodleDllHandle);
		OodleDllHandle = nullptr;
	}
#endif //PLATFORM_WINDOWS
}

bool FOodleModule::Uncompress(void* UncompressedBuffer, int32& UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, int32 CompressionData)
{
	if (OodleLZ_Decompress)
	{
		int32 Result = OodleLZ_Decompress(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		if (Result > 0)
		{
			UncompressedSize = Result;
			return true;
		}
	}
	
	return false;
}
