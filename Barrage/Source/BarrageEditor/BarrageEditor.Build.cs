// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class BarrageEditor : ModuleRules
{
	public BarrageEditor(ReadOnlyTargetRules Target) : base(Target)
	{

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(PluginDirectory,"Source/BarrageEditor/Public")
			});

		PrivateIncludePaths.AddRange(
			new string[]
			{
				Path.Combine(PluginDirectory,"Source/BarrageEditor/Private")
			});

		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/Barrage")
			}
		);


		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				Path.Combine(ModuleDirectory,"../JoltPhysics"), // for jolt includes
			}
		);


		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				Path.Combine(ModuleDirectory,"../JoltPhysics") // for jolt includes
			}
		);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Chaos",
				"JoltPhysics",
				"Engine",
				"UnrealEd",
				"JoltPhysics",
				"Barrage",
				"Kismet",
				"GeometryCore",
				"LocomoCore",
				"SkeletonKey",
				"mimalloc", "RenderCore", "Renderer" // <- add jolt dependecy here
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
				"SkeletonKey",
				"UnrealEd",
				"Kismet",
				"Barrage",
				"DeveloperSettings",
				"LocomoCore",
				"mimalloc",  "RenderCore", "Renderer", "RHICore", "RHI" // <- add jolt dependecy here
				// ... add private dependencies that you statically link with here ...	
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		SetupGameplayDebuggerSupport(Target);
	}
}