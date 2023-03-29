using UnrealBuildTool;


public class DynamicTree : ModuleRules{
	public DynamicTree(ReadOnlyTargetRules Target) : base(Target){
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
		});
		PrivateDependencyModuleNames.AddRange(new string[]{
		});

		if(Target.bBuildEditor){
			PrivateDependencyModuleNames.AddRange(new string[]{
				"UnrealEd",
			});
		}
	}
}

