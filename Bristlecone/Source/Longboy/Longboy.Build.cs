// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class Longboy : ModuleRules
{

    public Longboy(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicIncludePaths.AddRange(
            new string[] {
                Path.Combine(PluginDirectory,"Source/Longboy/Public")
            }
        );

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{

			PublicSystemLibraries.AddRange(
				new string[] {
					"shlwapi.lib",
					"crypt32.lib"
				}
			);

		}

		PublicDependencyModuleNames.AddRange(
			new string[] {
					"Core",
					"MsQuic",
					"MsQuicRuntime"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
					"Messaging",
					"Networking",
					"SSL"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
					Path.Combine(PluginDirectory,"Source/Longboy/Public"),
					Path.Combine(PluginDirectory,"Source/Longboy/Private"),
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
    }
}
