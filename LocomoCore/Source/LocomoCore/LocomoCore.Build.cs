// Copyright Oversized Sun. All Rights Reserved.
// LocomoCore is subject to the GPLv3 license.
// LocomoCore is a client of the Locomo library.

using System;
using System.IO;
using UnrealBuildTool;

public class LocomoCore : ModuleRules
{
	public LocomoCore(ReadOnlyTargetRules Target) : base(Target)
	{
		//PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		bEnableExceptions = true;
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/LocomoCore"),
				Path.Combine(PluginDirectory,"Source/LocomoCore/Public"),
				Path.Combine(PluginDirectory,"Source/LocomoCore/Public/Distances") //we only add this for back compat.

			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/LocomoCore/Private"),
			}
			);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"ApplicationCore", 
				"SkeletonKey", 
				"GameplayTags", "Eigen"
				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"ApplicationCore", "SkeletonKey", "GameplayTags"
				// ... add private dependencies that you statically link with here ...	
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
