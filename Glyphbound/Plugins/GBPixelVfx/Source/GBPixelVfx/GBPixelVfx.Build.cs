using UnrealBuildTool;

public class GBPixelVfx : ModuleRules
{
	public GBPixelVfx(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "Json", "ImageWrapper"
		});
	}
}
