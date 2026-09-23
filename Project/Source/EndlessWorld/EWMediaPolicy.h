#pragma once
#include "CoreMinimal.h"
#include "EWLocalization.h"

#ifndef EW_WITH_YOUTUBE
#error EW_WITH_YOUTUBE must be set explicitly by EndlessWorld.Build.cs
#endif

namespace EWMediaPolicy
{
inline constexpr bool PlaybackEnabled = EW_WITH_YOUTUBE != 0;
inline FString Unavailable()
{
    return EWL::Pick(TEXT("動画再生は公開版では利用できません。"),
        TEXT("Video playback is unavailable in this public build."));
}
inline FString Badge()
{
    return EWL::Pick(TEXT("公開版では利用不可"), TEXT("Unavailable in public build"));
}
}
