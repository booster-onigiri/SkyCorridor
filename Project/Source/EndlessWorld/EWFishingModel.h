#pragma once
#include "CoreMinimal.h"
#include "EWWorld.h"

namespace EWFishing
{
enum class EPhase : uint8 { Idle, Waiting, Bite, Reeling, Landed, Escaped };
struct FSpecies
{
    FString Id, Name, Description;
    uint8 Sites=7, Hours=15; // site bits; dawn 05-09, day 09-17, dusk 17-21, night 21-05
    int32 Weight=1;
    double Minimum=5, Maximum=30;
    FLinearColor Colour;
    double Slenderness=1;
};
struct FSpot
{
    FString Id, Name, Guide;
    EW::ChunkCoord Coord;
    FVector Stand, Float;
    double Yaw=0;
};
struct FCatch
{
    FString Id, Species, UTC;
    int32 Site=-1;
    double Length=0, Hour=0;
    bool Valid() const;
};
struct FRecord
{
    FString Species;
    int64 Count=0;
    double Largest=0;
};
const TArray<FSpecies>& Species();
const TArray<FSpot>& Spots();
inline FVector DisplayPosition(){return {6140,5260,2250};}
const FSpecies* Find(const FString& Id);
uint8 HourBand(double Hour);
FString TimeDescription(uint8 Bands);
FString SiteDescription(uint8 Sites);
bool Available(const FSpecies& Fish,int32 Site,double Hour);
FCatch Roll(int32 Site,double Hour,FRandomStream& Random,const FString& Id);

// Real gameplay state machine. Time is monotonic and independent of frame rate.
// A landed receipt remains stable until acknowledged after durable local storage.
class FCast
{
public:
    bool Begin(int32 Site,double Hour,double Now,bool Patient,FRandomStream& Random);
    bool Press(double Now);
    void Tick(double Now);
    void Cancel();
    EPhase Phase() const { return State; }
    const FCatch& Result() const { return Fish; }
    double Progress(double Now) const;
    bool Active() const { return State==EPhase::Waiting || State==EPhase::Bite || State==EPhase::Reeling; }
private:
    EPhase State=EPhase::Idle;
    FCatch Fish;
    double BiteAt=0, Deadline=0, LandAt=0;
    bool bPatient=false;
};
}
