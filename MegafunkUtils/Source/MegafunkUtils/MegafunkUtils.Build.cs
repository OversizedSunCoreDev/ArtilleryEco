// Some copyright should be here...

using UnrealBuildTool;

public class MegafunkUtils : ModuleRules
{
	public MegafunkUtils(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
			}
			);
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"ControlRig",
			}
			);
		
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimGraph", // In editor builds we try to validate anim graphs
				}
			);
		}
		
		CppCompileWarningSettings.NonInlinedGenCppWarningLevel = WarningLevel.Error;
	}
}
