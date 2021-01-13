// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Oodle : ModuleRules
{
	public Oodle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string DllName = "oo2core_8_win64.dll";
			string ToPath = Path.Combine(EngineDirectory, "Binaries", "Win64", DllName);
			string FromPath = Path.Combine(ModuleDirectory, "3rd", DllName);

			if (!File.Exists(ToPath))
			{
				File.Copy(FromPath, ToPath, true);
				System.Console.WriteLine(string.Format("Copy {0} from {1} to {2}", DllName, FromPath, ToPath));
			}
		}
	}
}
