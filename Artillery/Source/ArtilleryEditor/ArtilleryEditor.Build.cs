// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;
public class ArtilleryEditor : ModuleRules
{
	public ArtilleryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		//PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(PluginDirectory,"Source/ArtilleryEditor/Public")
			});
		
		PrivateIncludePaths.AddRange(
			new string[] 
			{
				Path.Combine(PluginDirectory,"Source/ArtilleryEditor/Private")
			});

		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime")
			}
		);

				
		
		RuntimeDependencies.Add(
			Path.Combine(PluginDirectory,"Data")
			);

		DirectoryReference m = DirectoryReference.FromString(Path.Combine(PluginDirectory, "Data"));
		if (m != null)
		{
			ConditionalAddModuleDirectory(m);
		}
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
				"Bristlecone",
				"ArtilleryRuntime",
				"Kismet",
				"UnrealEd"
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
				"ArtilleryRuntime",
				"Bristlecone",
				"Kismet",
				"UnrealEd",
				"ComponentVisualizers"
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
