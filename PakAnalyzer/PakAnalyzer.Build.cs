// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PakAnalyzer : ModuleRules
{
	public PakAnalyzer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Json",
				"AssetRegistry",
				"CoreUObject",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"PakFile",
			}
		);
	}
}
