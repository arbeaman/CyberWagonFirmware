using UnrealBuildTool;

public class CyberWagonFirmware : ModuleRules
{
	public CyberWagonFirmware(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"PhysicsCore",
			"ChaosVehicles",
			"ChaosVehiclesCore",
			"GameplayTags",
			"SlateCore",
			"Slate",
			"UMG",
		});

		PublicDependencyModuleNames.AddRange(new[] { "FactoryGame", "SML" });
	}
}
