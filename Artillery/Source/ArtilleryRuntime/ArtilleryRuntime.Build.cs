// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class ArtilleryRuntime : ModuleRules
{
	public ArtilleryRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		//PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/EssentialTypes/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/BasicTypes/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/Systems/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/Systems/Threads"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/TestTypes/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/Ticklites/"),
				Path.Combine(PluginDirectory,"Source/ArtilleryRuntime/Public/Components/")
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
				"JoltPhysics", 
				"MegafunkUtils",
				"ImGui",
				"DeveloperSettings"

				// ... add other public dependencies that you statically link with here ...
			}
			);
		
		if (Target.Type == TargetType.Editor)
		{
			PublicDependencyModuleNames.AddRange(new string[] { "BarrageEditor" });
		}

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"TraceLog",
				"GameplayAbilities",
				"GameplayTasks",
				"GameplayTags",
				"Bristlecone",
				"SkeletonKey", 
				"Barrage", 
				"LocomoCore",
				"Landscape",
				"Sockets",
				"Networking",
				"DeveloperSettings"
				// ... add private dependencies that you statically link with here ...
			}
			);
		
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd"
			});
		}
	}
}
