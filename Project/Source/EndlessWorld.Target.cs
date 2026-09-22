using UnrealBuildTool;
using System.Collections.Generic;
public class EndlessWorldTarget : TargetRules
{
    public EndlessWorldTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("EndlessWorld");
    }
}
