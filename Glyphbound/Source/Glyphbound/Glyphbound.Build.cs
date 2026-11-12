using UnrealBuildTool;

public class Glyphbound : ModuleRules
{
	public Glyphbound(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Every folder under Source/Glyphbound is an include root: #include "Core/GBTypes.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"Json", "JsonUtilities", "AIModule", "NavigationSystem",
			"Slate", "SlateCore", "UMG"
		});
	}
}
