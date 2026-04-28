// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class JoltPhysics : ModuleRules
{
	// enables JPH_DEBUG_RENDERER
	static bool bDebugDraw = true;
	
	// With this enabled keep in mind this only affects Jolt.
	// YOUR code is going to have to be deterministic too if it influences the outcome of Jolt and you expect the same results
	static bool bDeterministicJolt = true; 
	
	// If you don't want to use a custom allocator jolt can define them directly. It might be wiser for us to just define those as a macro?
	static bool bUseCustomAllocator = false; 
	
	static bool bSlowerDebugCode = false;
	static bool bProfiling = true;
	static bool bBroadPhaseStats = false; // I believe this assumes profiling is on 

	// This module essentialy mimics how Jolt builds with Cmake but from unrealbuildtool. If things change this might need to as well
	public JoltPhysics(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/JoltPhysics"),
				Path.Combine(PluginDirectory,"Source/JoltPhysics/Jolt")
			}
		);
		PublicIncludePaths.Add(ModuleDirectory);

		// This is a PCH only used in this module
		PrivatePCHHeaderFile = "Jolt/Jolt.h";

		// this thing redefines uint64 and other POD math types unfortunately (how rude)
#if UE_5_6_OR_LATER
		CppCompileWarningSettings.ShadowVariableWarningLevel = WarningLevel.Off;
#else
		ShadowVariableWarningLevel = WarningLevel.Off;
#endif
		if (Target.LinkType != TargetLinkType.Monolithic)
		{
			// Other modules want a shared lib
			PublicDefinitions.Add("JPH_SHARED_LIBRARY");
			// not only IS this a shared lib, but we are also building it!
			// This makes sure the API macros flip the right way similar to UE's
			PrivateDefinitions.Add("JPH_BUILD_SHARED_LIBRARY");
		}
		

		
		// Even during development it's kind of painful for every single sweep to be 10x slower.
		// Enable bSlowerDebugCode when actually debugging Jolt internals
		if (bSlowerDebugCode && Target.Configuration <= UnrealTargetConfiguration.DebugGame)
		{
			PublicDefinitions.Add("JPH_ENABLE_ASSERTS");
		}
		else
		{
			// remove debug things like asserts
			PublicDefinitions.Add("JPH_NO_DEBUG");
			PublicDefinitions.Add("JPH_DISABLE_CUSTOM_ALLOCATOR");
			OptimizeCode = CodeOptimization.Always;
			OptimizationLevel = OptimizationMode.Speed;
		}

		// JPH_DISABLE_TEMP_ALLOCATOR for ASAN to work apparently? https://github.com/jrouwe/JoltPhysics/blob/master/Build/README.md
		if (Target.WindowsPlatform.bEnableAddressSanitizer || !bUseCustomAllocator)
		{
			PublicDefinitions.Add("JPH_DISABLE_CUSTOM_ALLOCATOR");
		}
		
		if (Target.Configuration <= UnrealTargetConfiguration.Test)
		{
			if (bDebugDraw) 
			{
				PublicDefinitions.Add("JPH_DEBUG_RENDERER");
			}
			
			if (bProfiling)
			{
				PublicDefinitions.Add("JPH_EXTERNAL_PROFILE"); //mutually exclusive with JPH_PROFILE_ENABLED?
				
				if (bBroadPhaseStats)
				{
					PublicDefinitions.Add("JPH_TRACK_BROADPHASE_STATS");
				}
			}
		}
		
		// whether we are going deterministic mode or not
		if (bDeterministicJolt)
		{
			PublicDefinitions.Add("JPH_CROSS_PLATFORM_DETERMINISTIC");
			// From jolt docs:
			// "Compile your application mode in Precise mode (clang: -ffp-model=precise, MSVC: /fp:precise)"
			FPSemantics = FPSemanticsMode.Precise;
			// From unreal build warnings:
			// "Overriding FPSemantics requires a private PCH"
			PCHUsage = PCHUsageMode.NoSharedPCHs;
		}
		
		// public to match on both sides
		PublicDefinitions.Add("JPH_OBJECT_STREAM");  // Use the object stream
		PublicDefinitions.Add("JPH_OBJECT_LAYER_BITS=16"); // Jolt can use a smaller 16 bit object layer. If you need more raise this
		
		// Intrinsics that get handled based on other settings
		// (for example JPH_CROSS_PLATFORM_DETERMINISTIC will make most of these unused, but the cmake setup expects some of these)
		PublicDefinitions.Add("JPH_USE_SSE4_2");
		PublicDefinitions.Add("JPH_USE_SSE4_1");
		PublicDefinitions.Add("JPH_USE_LZCNT");
		PublicDefinitions.Add("JPH_USE_TZCNT");
		PublicDefinitions.Add("JPH_USE_F16C");
		PublicDefinitions.Add("JPH_USE_AVX");
		PublicDefinitions.Add("JPH_USE_AVX2");
    }
}
