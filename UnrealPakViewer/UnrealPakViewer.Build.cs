// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using Tools.DotNETCommon;

using System.IO;

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
				"EditorStyle",
				"PakAnalyzer",
				"Json",
				"ImageWrapper",
				"Oodle",
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
			Log.TraceInformation("UnrealPakViewer is outside engine source directory, parent project is: {0}", ProjectName);

			PrivateDefinitions.Add(string.Format("ParentProjectName=TEXT(\"{0}\")", ProjectName));
		}
	}
}
