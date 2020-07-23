#include "SAboutWindow.h"

#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Launch/Resources/Version.h"
#include "Misc/FileHelper.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/SlateBrush.h"

#include "Models/Alipay.png.h"
#include "Models/WechatPay.png.h"
#include "SKeyValueRow.h"

#define LOCTEXT_NAMESPACE "SAboutWindow"

struct FDynamicImageBrush : public FSlateBrush
{
public:
	FORCENOINLINE FDynamicImageBrush(const FString& InImageName, const FVector2D& InImageSize, const FLinearColor& InTint = FLinearColor(1, 1, 1, 1), ESlateBrushTileType::Type InTiling = ESlateBrushTileType::NoTile, ESlateBrushImageType::Type InImageType = ESlateBrushImageType::FullColor)
		: FSlateBrush(ESlateBrushDrawType::Image, *InImageName, FMargin(0), InTiling, InImageType, InImageSize, InTint, nullptr, true)
	{ }
};

static FDynamicImageBrush* AliBrush = new FDynamicImageBrush(TEXT("DONATE_ALIPAY_PNG"), FVector2D(200, 200));
static FDynamicImageBrush* WechatBrush = new FDynamicImageBrush(TEXT("DONATE_WECHAT_PNG"), FVector2D(200, 200));

SAboutWindow::SAboutWindow()
{
	uint32 ImageWidth = 0, ImageHeight = 0;
	TArray<uint8> RawData;

	if (LoadTexture(ALIPAY_DATA, ImageWidth, ImageHeight, RawData))
	{
		FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(TEXT("DONATE_ALIPAY_PNG"), ImageWidth, ImageHeight, RawData);
	}

	RawData.Empty();
	if (LoadTexture(WECHATPAY_DATA, ImageWidth, ImageHeight, RawData))
	{
		FSlateApplication::Get().GetRenderer()->GenerateDynamicImageResource(TEXT("DONATE_WECHAT_PNG"), ImageWidth, ImageHeight, RawData);
	}
}

SAboutWindow::~SAboutWindow()
{
}

void SAboutWindow::Construct(const FArguments& Args)
{
	const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
	const FVector2D InitialWindowDimensions(600, 290);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "About"))
		.HasCloseButton(true)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize)
		.ClientSize(InitialWindowDimensions * DPIScaleFactor)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
			.Padding(FMargin(5.f, 10.f))
			[
				SNew(SVerticalBox)

#if defined(UNREAL_PAK_VIEWER_VERSION)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SKeyValueRow).KeyStretchCoefficient(0.2f).KeyText(LOCTEXT("Version_Text", "Version:")).ValueText(FText::FromString(UNREAL_PAK_VIEWER_VERSION))
				]
#endif

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SKeyValueRow).KeyStretchCoefficient(0.2f).KeyText(LOCTEXT("Engine_Text", "Engine:")).ValueText(FText::Format(LOCTEXT("Engine_Version_Text", "{0}.{1}.{2}"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				//.HAlign(HAlign_Center)
				.Padding(0.f, 4.f)
				[
					SNew(SExpandableArea)
					.InitiallyCollapsed(false)
					.AreaTitle(LOCTEXT("DonateText", "Help me make UnrealPakViewer better!"))
					.OnAreaExpansionChanged(this, &SAboutWindow::OnDonateAreaExpansionChanged)
					.BodyContent()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						//.Padding(0.f, 0.f, 20.f, 0.f)
						.FillWidth(1.f)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.AutoHeight()
							.Padding(0.f, 4.f)
							[
								SNew(SImage).Image(AliBrush)
							]

							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("Alipay_Text", "Ali Pay"))
							]
						]

						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.AutoHeight()
							.Padding(0.f, 4.f)
							[
								SNew(SImage).Image(WechatBrush)
							]

							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock).Text(LOCTEXT("WechetPay_Text", "Wechat Pay"))
							]
						]
					]
				]
			]
		]
	);
}

bool SAboutWindow::LoadTexture(const FString& ResourcePath, uint32& OutWidth, uint32& OutHeight, TArray<uint8>& OutDecodedImage)
{
	uint32 BytesPerPixel = 4;
	bool bSucceeded = true;
	TArray<uint8> RawFileData;
	if (FFileHelper::LoadFileToArray(RawFileData, *ResourcePath))
	{
		return LoadTexture(RawFileData, OutWidth, OutHeight, OutDecodedImage);
	}
	else
	{
		//UE_LOG(LogSlateD3D, Log, TEXT("Could not find file for Slate resource: [%s] '%s'"), *InBrush.GetResourceName().ToString(), *ResourcePath);
		bSucceeded = false;
	}

	return bSucceeded;
}

bool SAboutWindow::LoadTexture(const TArray<uint8>& RawFileData, uint32& OutWidth, uint32& OutHeight, TArray<uint8>& OutDecodedImage)
{
	uint32 BytesPerPixel = 4;
	bool bSucceeded = true;

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	//Try and determine format, if that fails assume PNG
	EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(RawFileData.GetData(), RawFileData.Num());
	if (ImageFormat == EImageFormat::Invalid)
	{
		ImageFormat = EImageFormat::PNG;
	}
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);

	if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(RawFileData.GetData(), RawFileData.Num()))
	{
		OutWidth = ImageWrapper->GetWidth();
		OutHeight = ImageWrapper->GetHeight();

#if ENGINE_MINOR_VERSION < 25
		const TArray<uint8>* RawData = NULL;
		if (ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, RawData) == false)
#else
		if (ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, OutDecodedImage) == false)
#endif
		{
			//UE_LOG(LogSlateD3D, Log, TEXT("Invalid texture format for Slate resource only RGBA and RGB pngs are supported: %s"), *InBrush.GetResourceName().ToString());
			bSucceeded = false;
		}
#if ENGINE_MINOR_VERSION < 25
		else
		{
			OutDecodedImage = *RawData;
		}
#endif
	}
	else
	{
		//UE_LOG(LogSlateD3D, Log, TEXT("Only pngs are supported in Slate. [%s] '%s'"), *InBrush.GetResourceName().ToString(), *ResourcePath);
		bSucceeded = false;
	}

	return bSucceeded;
}

void SAboutWindow::OnDonateAreaExpansionChanged(bool bExpand)
{
	static const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
	const FVector2D WindowDimensions = bExpand ? FVector2D(600, 290) : FVector2D(600, 70);

	Resize(DPIScaleFactor * WindowDimensions);
}

#undef LOCTEXT_NAMESPACE
