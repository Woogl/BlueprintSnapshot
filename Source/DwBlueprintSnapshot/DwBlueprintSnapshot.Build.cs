// Copyright Woogle. All Rights Reserved.

using UnrealBuildTool;

public class DwBlueprintSnapshot : ModuleRules
{
	public DwBlueprintSnapshot(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
				"UnrealEd",
				"Kismet",
				"BlueprintGraph",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
				"AssetRegistry",
				"Projects",
				"UMG",
				"UMGEditor",
				"Blutility",
				"ModelViewViewModel",
				"ModelViewViewModelBlueprint",
			}
		);
	}
}
