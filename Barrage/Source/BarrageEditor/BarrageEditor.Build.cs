// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class BarrageEditor : ModuleRules
{
	public BarrageEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
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
				"GeometryCore",
				"SkeletonKey",
				"mimalloc" // <- add jolt dependecy here
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
				"SkeletonKey",
				"mimalloc"// <- add jolt dependecy here
				// ... add private dependencies that you statically link with here ...	
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	
		ExternalDependencies.Add(Path.Combine(ModuleDirectory, "../JoltPhysics")); // checks to determine if jolt needs to be rebuilt
	
	

        // JOLT Stuff
        PublicDefinitions.Add("JPH_CROSS_PLATFORM_DETERMINISTIC");
        //PublicDefinitions.Add("JPH_DOUBLE_PRECISION");
        PublicDefinitions.Add("JPH_OBJECT_STREAM"); 
        PublicDefinitions.Add("JPH_OBJECT_LAYER_BITS=16");
        PublicDefinitions.Add("JPH_USE_SSE4_2");
        PublicDefinitions.Add("JPH_USE_SSE4_1");
        PublicDefinitions.Add("JPH_USE_LZCNT");
        PublicDefinitions.Add("JPH_USE_TZCNT");
        PublicDefinitions.Add("JPH_USE_F16C");


        var configType = "";

        if (Target.Configuration == UnrealTargetConfiguration.Debug)
        {
            PublicDefinitions.Add("JPH_PROFILE_ENABLED");
            PublicDefinitions.Add("JPH_DEBUG_RENDERER");
            PublicDefinitions.Add("JPH_ENABLE_ASSERTS");
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                //PublicDefinitions.Add("JPH_FLOATING_POINT_EXCEPTIONS_ENABLED");
            }
            configType = "Debug";
        }
        else if (Target.Configuration == UnrealTargetConfiguration.DebugGame || Target.Configuration == UnrealTargetConfiguration.Development)
        {
            PublicDefinitions.Add("JPH_PROFILE_ENABLED");
            PublicDefinitions.Add("JPH_DEBUG_RENDERER");
            PublicDefinitions.Add("JPH_ENABLE_ASSERTS");

            configType = "Release";
        }
        else
        {
            configType = "Distribution";
        }

        
        var libPath = "";

        // only for win64. but shouldn't be a problem to do it for other platforms, you just need to change the path
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
			// this path is relative and can change a bit, adjust it according to your project structure. you need to point to library what is built by UE4CMake (*.lib / *.a)
			// TODO: replace this. It's a huge maintenance hazard.
            libPath = Path.Combine(ModuleDirectory, "../../../../Intermediate/CMakeTarget/Jolt/build/Jolt/" + configType, "Jolt.lib");
        }


        PublicAdditionalLibraries.Add(libPath);
		
	}
}
