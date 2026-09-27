using UnrealBuildTool;

public class ArenaGameEditorTarget : TargetRules
{
	public ArenaGameEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("ArenaGame");
	}
}
