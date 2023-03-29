using UnrealBuildTool;
using System.Collections.Generic;


public class DynamicTreeEditorTarget : TargetRules{
	public DynamicTreeEditorTarget(TargetInfo Target) : base(Target){
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_1;
		ExtraModuleNames.Add("DynamicTree");
	}
}

