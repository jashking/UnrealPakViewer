#include "UnrealPakViewerStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH(ResourceDir, RelativePath, ...)  FSlateImageBrush (ResourceDir / RelativePath + TEXT(".png"), __VA_ARGS__)

TSharedPtr<FSlateStyleSet> FUnrealPakViewerStyle::StyleInstance;

void FUnrealPakViewerStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FUnrealPakViewerStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	StyleInstance.Reset();
}

FName FUnrealPakViewerStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("UnrealPakViewerStyle"));
	return StyleSetName;
}

const ISlateStyle& FUnrealPakViewerStyle::Get()
{
	return *StyleInstance;
}

TSharedRef<FSlateStyleSet> FUnrealPakViewerStyle::Create()
{
	TSharedRef<FSlateStyleSet> StyleRef = MakeShareable<FSlateStyleSet>(new FSlateStyleSet(FUnrealPakViewerStyle::GetStyleSetName()));

	//const FString ResourceDir = FPaths::ProjectDir() / TEXT("Resources");
	const FString ResourceDir = FPaths::EngineContentDir() / TEXT("Editor/Slate");

	FSlateStyleSet& Style = StyleRef.Get();

	Style.Set("FolderClosed", new IMAGE_BRUSH(ResourceDir, "/Icons/FolderClosed", FVector2D(18.f, 16.f)));
	Style.Set("FolderOpen", new IMAGE_BRUSH(ResourceDir, "/Icons/FolderOpen", FVector2D(18.f, 16.f)));
	Style.Set("Extract", new IMAGE_BRUSH(ResourceDir, "/Icons/icon_FontEd_Export_40x", FVector2D(40.f, 40.f)));
	Style.Set("Find", new IMAGE_BRUSH(ResourceDir, "/Icons/icon_Blueprint_Find_40px", FVector2D(40.f, 40.f)));
	Style.Set("LoadPak", new IMAGE_BRUSH(ResourceDir, "/Icons/icon_file_open_40x", FVector2D(40.f, 40.f)));
	Style.Set("Copy", new IMAGE_BRUSH(ResourceDir, "/Icons/Edit/icon_Edit_Copy_40x", FVector2D(40.f, 40.f)));
	Style.Set("View", new IMAGE_BRUSH(ResourceDir, "/Icons/icon_levels_visible_40x", FVector2D(40.f, 40.f)));
	Style.Set("Export", new IMAGE_BRUSH(ResourceDir, "/Icons/icon_ContentBrowser_40x", FVector2D(40.f, 40.f)));
	Style.Set("Tab.Tree", new IMAGE_BRUSH(ResourceDir, "/Icons/icon_Persona_Skeleton_Tree_16x", FVector2D(20.f, 20.f)));
	Style.Set("Tab.File", new IMAGE_BRUSH(ResourceDir, "/Icons/icon_source_control_40x", FVector2D(20.f, 20.f)));
	Style.Set("Tab.Summary", new IMAGE_BRUSH(ResourceDir, "/Icons/icon_StaticMeshEd_Collision_40x", FVector2D(20.f, 20.f)));

	return StyleRef;
}

#undef IMAGE_BRUSH
