// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class ArtilleryRuntime : ModuleRules
{
	public ArtilleryRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		//PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		bEnableExceptions = true;
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/EssentialTypes/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/BasicTypes/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/Systems/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/Systems/Threads"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/TestTypes/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/Ticklites/")
			}
		);
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
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
				"InputCore",
                "SlateCore",
				"GameplayAbilities",
				"GameplayTags",
				"Bristlecone",
				"SkeletonKey",
				"Barrage",
				"Cabling",
				"LocomoCore",
				"Niagara", 
				"Niagara", 
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
				"SlateCore",
				"GameplayAbilities",
				"GameplayTasks",
				"GameplayTags",
				"Bristlecone",
				"SkeletonKey", 
				"Barrage", 
				"LocomoCore",
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
