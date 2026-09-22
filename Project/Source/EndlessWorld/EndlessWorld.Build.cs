using UnrealBuildTool;
using System.IO;
using EpicGames.Core;
public class EndlessWorld : ModuleRules
{
    public EndlessWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "SQLiteCore", "Json", "JsonUtilities", "Slate", "SlateCore", "UMG", "RenderCore", "RHI", "ApplicationCore", "PhysicsCore" });
        PrivateDependencyModuleNames.AddRange(new string[] { "EWGamepad", "MovieSceneCapture" });
        // Installing a plugin must not silently change the public baseline.
        // The setup tool updates this explicit profile and plugin enablement.
        string ProjectFile = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../EndlessWorld.uproject"));
        ExternalDependencies.Add(ProjectFile);
        JsonObject Descriptor = JsonObject.Read(new FileReference(ProjectFile));
        string Profile = Descriptor.TryGetStringField("EWGraphicsProfile", out string ProfileValue)
            ? ProfileValue : "baseline";
        if (Profile != "baseline" && Profile != "nvidia")
            throw new BuildException("Unknown EWGraphicsProfile. Run Tools/configure-graphics.py baseline or nvidia.");
        bool WithNvidia = Profile == "nvidia";
        if (WithNvidia && Target.Platform != UnrealTargetPlatform.Win64)
            throw new BuildException("The optional NVIDIA profile currently supports Win64 only. Select baseline for this target.");
        if (!Descriptor.TryGetObjectArrayField("Plugins", out JsonObject[] Plugins))
            throw new BuildException("Missing Plugins array. Run Tools/configure-graphics.py " + Profile + ".");
        foreach (string PluginName in new[] { "DLSS", "StreamlineCore", "StreamlineNGXCommon", "StreamlineDLSSG", "StreamlineReflex" })
        {
            bool Found = false;
            foreach (JsonObject Plugin in Plugins)
            {
                if (!Plugin.TryGetStringField("Name", out string Name) || Name != PluginName) continue;
                Found = Plugin.TryGetBoolField("Enabled", out bool Enabled) && Enabled == WithNvidia;
                break;
            }
            if (!Found)
                throw new BuildException("Graphics profile and " + PluginName + " enablement differ. Run Tools/configure-graphics.py " + Profile + ".");
        }
        PublicDefinitions.Add("EW_WITH_NVIDIA=" + (WithNvidia ? "1" : "0"));
        if (WithNvidia)
            PrivateDependencyModuleNames.AddRange(new string[] { "DLSSBlueprint", "NGXRHI", "StreamlineBlueprint", "StreamlineDLSSGBlueprint", "StreamlineReflexBlueprint", "StreamlineDXGIRHI" });
        if (Target.Platform == UnrealTargetPlatform.Win64) PublicSystemLibraries.AddRange(new string[] { "d2d1.lib", "dwrite.lib", "gdi32.lib", "user32.lib" });
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "CEF3Utils", "HTTP", "AudioMixer", "AudioExtensions", "WebSockets", "EOSShared", "EOSVoiceChat", "VoiceChat" });
            AddEngineThirdPartyPrivateStaticDependencies(Target, "EOSSDK");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "CEF3");
            RuntimeDependencies.Add("$(EngineDir)/Binaries/Win64/EpicWebHelper.exe");
            // Optional room-host runtime. Never stage tests, captured media, or
            // arbitrary files from this directory. Solo/YouTube needs none of it.
            string CinemaRuntime = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../CinemaOnline"));
            foreach (string RelativeFile in new[] {
                "server.mjs", "node.exe", "cloudflared.exe", "LICENSE-node.txt", "LICENSE-cloudflared.txt",
                "node_modules/ws/package.json", "node_modules/ws/index.js", "node_modules/ws/wrapper.mjs", "node_modules/ws/LICENSE",
                "node_modules/ws/lib/buffer-util.js", "node_modules/ws/lib/constants.js", "node_modules/ws/lib/event-target.js",
                "node_modules/ws/lib/extension.js", "node_modules/ws/lib/limiter.js", "node_modules/ws/lib/permessage-deflate.js",
                "node_modules/ws/lib/receiver.js", "node_modules/ws/lib/sender.js", "node_modules/ws/lib/stream.js",
                "node_modules/ws/lib/subprotocol.js", "node_modules/ws/lib/validation.js", "node_modules/ws/lib/websocket.js",
                "node_modules/ws/lib/websocket-server.js" })
            {
                if (File.Exists(Path.Combine(CinemaRuntime, RelativeFile)))
                    RuntimeDependencies.Add("$(ProjectDir)/CinemaOnline/" + RelativeFile, StagedFileType.NonUFS);
            }
        }
        string WorkshopRuntime = System.IO.Path.GetFullPath(System.IO.Path.Combine(ModuleDirectory, "../../WorldWorkshop"));
        if (Directory.Exists(WorkshopRuntime))
            foreach (string RuntimeFile in Directory.GetFiles(WorkshopRuntime, "*.json", SearchOption.AllDirectories))
                RuntimeDependencies.Add("$(ProjectDir)/WorldWorkshop/" + Path.GetRelativePath(WorkshopRuntime, RuntimeFile).Replace('\\', '/'), StagedFileType.NonUFS);
        if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "MeshDescription", "StaticMeshDescription" });
    }
}
