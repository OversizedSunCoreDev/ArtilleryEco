// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class SunflowerRuntime : ModuleRules
{
	public SunflowerRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		bEnableExceptions = true;
		PublicIncludePaths.AddRange(
			new string[] {
				Path.Combine(PluginDirectory,"Source/SunflowerRuntime")
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
                "Barrage",
                "BarrageEditor",
                "SlateCore", 
                "SkeletonKey", 
                "LocomoCore",
                "Niagara",
                "NiagaraEditor",
                "NiagaraUIRenderer",
                "NiagaraUIRendererEditor",
                "ArtilleryRuntime",
                "ThistleRuntime",
                "MassEntity",
                "UMG"

				// ... add other public dependencies that you statically link with here ...
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "ApplicationCore",
                "InputCore",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "LocomoCore",
                "ArtilleryRuntime",
                "ThistleRuntime",
                "MassEntity"
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
