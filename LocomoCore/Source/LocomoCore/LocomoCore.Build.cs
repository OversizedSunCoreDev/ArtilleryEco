// Copyright Oversized Sun. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class LocomoCore : ModuleRules
{
	public LocomoCore(ReadOnlyTargetRules Target) : base(Target)
	{
		//PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		// .Build.cs — overlay MUST precede UE's Boost so our 3 headers shadow it
		PublicIncludePaths.Add(Path.Combine(PluginDirectory, "Source/LocomoCore/Private/SpatialIndexDeps"));
		AddEngineThirdPartyPrivateStaticDependencies(Target, "Boost");  // adds .../boost-1.85.0/include as a root
		
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/LocomoCore"),
				Path.Combine(PluginDirectory,"Source/LocomoCore/Public"),
				Path.Combine(PluginDirectory,"Source/LocomoCore/Public/Distances"), //we only add this for back compat.
				

			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/LocomoCore/Private"),
				Path.Combine(PluginDirectory, "Source/Private/SpatialIndexDeps")
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
				"GameplayTags", "Eigen", "Boost"
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
				"ApplicationCore", "SkeletonKey", "GameplayTags", "Boost"
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
