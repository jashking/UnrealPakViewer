// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
#if UE_5_0_OR_LATER
using EpicGames.Core;
#else
using Tools.DotNETCommon;
#endif

using System.IO;
using Microsoft.Extensions.Logging;

public class UnrealPakViewer : ModuleRules
{
	public UnrealPakViewer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Runtime/Launch/Public"));
		PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Runtime/Launch/Private"));      // For LaunchEngineLoop.cpp include

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AppFramework",
				"Core",
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"StandaloneRenderer",
				"DesktopPlatform",
				"Projects",
				//"EditorStyle",
				"PakAnalyzer",
				"Json",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnixCommonStartup"
				}
			);
		}

		DirectoryReference EngineSourceProgramsDirectory = new DirectoryReference(Path.Combine(EngineDirectory, "Source", "Programs"));
		FileReference CurrentModuleDirectory = new FileReference(ModuleDirectory);
		if (!CurrentModuleDirectory.IsUnderDirectory(EngineSourceProgramsDirectory))
		{
			string ProjectName = Target.ProjectFile.GetFileNameWithoutExtension();
			Logger.LogInformation("UnrealPakViewer is outside engine source directory, parent project is: {ProjectName}", ProjectName);

			PrivateDefinitions.Add(string.Format("ParentProjectName=TEXT(\"{0}\")", ProjectName));
		}
	}
}
