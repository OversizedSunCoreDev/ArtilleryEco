// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Xml.Linq;
using UnrealBuildTool;

public class BarrageTests : ModuleRules
{
	public BarrageTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
			}
			);
			
		

		
		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(PluginDirectory,"Source/BarrageTests")
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"UnrealEd",
				"CoreUObject",
				"Chaos",
				"SkeletonKey",
				"Barrage",
				"JoltPhysics"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"UnrealEd",
				"CoreUObject",
				"Chaos",
				"SkeletonKey",
				"Barrage",
				"JoltPhysics"
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
