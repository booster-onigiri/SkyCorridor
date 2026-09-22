using UnrealBuildTool;
using System.IO;

public class EWGamepad : ModuleRules
{
    public EWGamepad(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "InputDevice", "ApplicationCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "Engine", "CoreUObject", "InputCore", "Projects", "Slate", "SlateCore" });
        string Sdl = Path.Combine(PluginDirectory, "ThirdParty", "SDL3");
        PublicSystemIncludePaths.Add(Path.Combine(Sdl, "include"));
        PublicAdditionalLibraries.Add(Path.Combine(Sdl, "SDL3.lib"));
        PublicDelayLoadDLLs.Add("SDL3.dll");
        RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Binaries", "ThirdParty", "Win64", "SDL3.dll"), Path.Combine(Sdl, "SDL3.dll"));
        RuntimeDependencies.Add(Path.Combine(Sdl, "LICENSE.txt"), StagedFileType.NonUFS);
    }
}
