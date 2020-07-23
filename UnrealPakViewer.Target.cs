// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UnrealPakViewerTarget : TargetRules
{
    public UnrealPakViewerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Program;
        LinkType = TargetLinkType.Monolithic;
        LaunchModuleName = "UnrealPakViewer";
        SolutionDirectory = "MyPrograms";
        DefaultBuildSettings = BuildSettingsVersion.V2;

        ExtraModuleNames.Add("EditorStyle");

        // Lean and mean
        bBuildDeveloperTools = true;

        // Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
        bCompileAgainstEngine = false;
        bCompileAgainstCoreUObject = true;

        bUseLoggingInShipping = true;
        bCompileWithPluginSupport = false;

        bHasExports = false;

		GlobalDefinitions.Add("NOINITCRASHREPORTER=1");

		GlobalDefinitions.Add(string.Format("UNREAL_PAK_VIEWER_VERSION=TEXT(\"{0}\")", "1.0"));
	}
}
