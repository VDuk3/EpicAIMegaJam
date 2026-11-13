using UnrealBuildTool;

public class GBPixelVfxBaker : ModuleRules
{
	public GBPixelVfxBaker(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "Json", "ImageWrapper",
			"Niagara", "GBPixelVfx", "RenderCore", "RHI"
		});
	}
}
