#pragma once
#include "CoreMinimal.h"

namespace EWWorldClock
{
inline constexpr double DaySeconds=1800.;
inline constexpr double LegacyDaySeconds=3600.;

inline double Advance(double Hour,double Elapsed,double Period=DaySeconds)
{
    const double Result=FMath::Fmod(Hour+FMath::Max(0.,Elapsed)*24./Period,24.);
    return Result<0?Result+24.:Result;
}

// Preserve the current sky when loading a save made with a different day length.
inline double RebaseEpoch(double Now,double Epoch,double PreviousPeriod)
{
    const double Phase=FMath::Fmod(FMath::Max(0.,Now-Epoch),PreviousPeriod)/PreviousPeriod;
    return Now-Phase*DaySeconds;
}
}
