// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
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
                "Slate",
                "SlateCore",
                "StandaloneRenderer",
                "PakFile",
                "DesktopPlatform",
                "Projects",
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
    }
}
