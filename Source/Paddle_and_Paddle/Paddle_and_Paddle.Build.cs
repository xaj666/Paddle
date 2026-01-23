// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Paddle_and_Paddle : ModuleRules
{
	public Paddle_and_Paddle(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"InputCore",
			"EnhancedInput" ,
			//"UnrealEd",
			"AssetRegistry",
			"MediaAssets",
			"Json",
			"JsonUtilities",
			"DeveloperSettings",
			"Slate",
			"SlateCore",
			"AudioMixer" });
	}	
}
