using UnrealBuildTool;
using System.Collections.Generic;

public class GlyphboundTarget : TargetRules
{
	public GlyphboundTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V8;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
		ExtraModuleNames.Add("Glyphbound");
	}
}
