// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;

public class ThistleRuntime : ModuleRules
{
	public ThistleRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
		PublicIncludePaths.AddRange([
			Path.Combine(PluginDirectory,"Source/ThistleRuntime"),
			Path.Combine(PluginDirectory,"Source/ThistleRuntime/Public/StateTree"),
		]);
		
		PrivateIncludePaths.AddRange([]);
		
		PublicDependencyModuleNames.AddRange([
			"Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "ApplicationCore",
            "InputCore",
            "SlateCore",
            "AIModule",
            "ArtilleryRuntime",
            "GameplayAbilities",
			"SkeletonKey",
			"Barrage",
			"Bristlecone",
			"NavigationSystem",
			"GameplayTags",
			"StateTreeModule",
			"MassSmartObjects",
			"SmartObjectsModule",
			"GameplayBehaviorsModule",
			"MassAIBehavior", 
			"GameplayStateTreeModule",
			"LocomoCore",
			"JoltPhysics",
		]);
		
		PrivateDependencyModuleNames.AddRange([
			"Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "ApplicationCore",
            "InputCore",
            "SlateCore",
            "AIModule",
            "ArtilleryRuntime",
            "Barrage",
            "NavigationSystem",
            "StateTreeModule",
            "GameplayStateTreeModule",
            "MassAIBehavior", "MegafunkUtils",
		]);
		
		DynamicallyLoadedModuleNames.AddRange([]);
	}
}
