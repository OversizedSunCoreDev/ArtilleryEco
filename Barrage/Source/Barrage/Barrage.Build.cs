// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class Barrage : ModuleRules
{
	public Barrage(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Chaos",
				"JoltPhysics", "GeometryCore", "SkeletonKey", "mimalloc", "LocomoCore" // <- add jolt dependecy here
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Chaos",
				"Slate",
				"SlateCore",
				"JoltPhysics",
				"LocomoCore",
				"SkeletonKey",
				"mimalloc",
				"RenderCore", "RHI" // Mainly needed for debug rendering, arguably optional in shipping builds
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
