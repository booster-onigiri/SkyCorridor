#include "EWMusic.h"
#include "EWLocalization.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWMediaScreen.h"
#include "EWSocialSession.h"
#include "EWTerminal.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"
#include "Dom/JsonObject.h"

AEWMusic::AEWMusic()
{
    PrimaryActorTick.bCanEverTick=true;
    Audio=CreateDefaultSubobject<UAudioComponent>(TEXT("QuietScore"));SetRootComponent(Audio);
    Audio->bAutoActivate=false;Audio->bAllowSpatialization=false;Audio->bIsUISound=false;
    Audio->bAutoDestroy=false;Audio->bShouldRemainActiveIfDropped=true;
}
void AEWMusic::BeginPlay()
{
    Super::BeginPlay();
    bSilent=FParse::Param(FCommandLine::Get(),TEXT("EWSilentAudit")) || FParse::Param(FCommandLine::Get(),TEXT("nosound"));
    GConfig->GetFloat(TEXT("EndlessWorld.Sound"),TEXT("MusicVolume"),UserVolume,GGameUserSettingsIni);
    UserVolume=FMath::IsFinite(UserVolume)?FMath::Clamp(UserVolume,0.f,1.f):.32f;
    for(const TCHAR* Name:{TEXT("CanalAfterglow"),TEXT("WindowWithoutVoices"),TEXT("LampOnTheWayHome")})
        Tracks.Add(LoadObject<USoundWave>(nullptr,*FString::Printf(TEXT("/Game/EndlessWorld/Audio89/%s.%s"),Name,Name)));
}
void AEWMusic::SetVolume(float Value)
{
    if(!FMath::IsFinite(Value))return;
    UserVolume=FMath::Clamp(Value,0.f,1.f);
    GConfig->SetFloat(TEXT("EndlessWorld.Sound"),TEXT("MusicVolume"),UserVolume,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
    if(UserVolume==0){Audio->Stop();Current=INDEX_NONE;Gain=0;Remaining=32;}
}
void AEWMusic::StartCue(int32 Index)
{
    if(!Tracks.IsValidIndex(Index) || !Tracks[Index] || UserVolume<=0)return;
    Current=Index;Next=(Index+1)%Tracks.Num();PlayingFor=0;Gain=0;++Started;
    Audio->Stop();Audio->SetSound(Tracks[Index]);Audio->SetVolumeMultiplier(0);
    if(!bSilent)Audio->Play();
}
void AEWMusic::PreviewCue(int32 Index)
{
    if(GetWorld()->IsPlayInEditor() || FParse::Param(FCommandLine::Get(),TEXT("EWMemory89Audit")))StartCue(Index);
}
void AEWMusic::Tick(float Delta)
{
    Super::Tick(Delta);auto* G=GetGameInstance<UEWGameInstance>();
    const bool Ready=G && G->SessionStarted() && G->Manager && !G->Manager->IsTravelling();
    bool Other=false;
    if(G)
    {
        for(auto* Screen:{G->MediaScreen.Get(),G->CinemaScreen.Get(),G->SkyTheatre.Get()})
            Other|=Screen && Screen->AudibleAtListener();
        Other|=G->SocialSession && G->SocialSession->HearingVoice();
        Other|=G->Terminal && G->Terminal->MediaAudible();
    }
    if(Other)DuckHold=3;else DuckHold=FMath::Max(0.,DuckHold-Delta);
    if(Current==INDEX_NONE)
    {
        if(Ready && UserVolume>0 && DuckHold<=0 && G->Menu()==EEWMenu::None)
        {Remaining-=Delta;if(Remaining<=0)StartCue(Next);}
        return;
    }
    PlayingFor+=Delta;
    const double Duration=Tracks[Current]->Duration;
    const float Envelope=FMath::Min(FMath::Clamp(PlayingFor/5.,0.,1.),FMath::Clamp((Duration-PlayingFor)/6.,0.,1.));
    const float Target=Ready && DuckHold<=0?UserVolume*Envelope:0;
    Gain=FMath::FInterpTo(Gain,Target,Delta,Target<Gain?4.f:.5f);
    Audio->SetVolumeMultiplier(Gain);
    if(PlayingFor>=Duration)
    {
        Audio->Stop();Current=INDEX_NONE;Gain=0;
        const double Quiet[]={110,165,135};Remaining=Quiet[(Started-1)%3];
    }
}
FString AEWMusic::Title() const
{
    const TCHAR* Names[]={TEXT("水路のあと"),TEXT("声のない窓"),TEXT("帰り道の灯")};
    return EWL::Translate(Current>=0 && Current<3?Names[Current]:TEXT("水と風の時間"));
}
TSharedRef<FJsonObject> AEWMusic::Evidence() const
{
    auto O=MakeShared<FJsonObject>();int32 Loaded=0;for(const auto& Track:Tracks)if(Track)++Loaded;
    O->SetNumberField(TEXT("tracks_loaded"),Loaded);O->SetNumberField(TEXT("current"),Current);
    O->SetNumberField(TEXT("started"),Started);O->SetNumberField(TEXT("gain"),Gain);O->SetNumberField(TEXT("volume"),UserVolume);
    O->SetNumberField(TEXT("quiet_seconds_remaining"),Remaining);O->SetNumberField(TEXT("cue_seconds"),PlayingFor);
    O->SetBoolField(TEXT("silent_test"),bSilent);O->SetBoolField(TEXT("component_playing"),Audio->IsPlaying());
    O->SetBoolField(TEXT("ducking"),DuckHold>0);return O;
}
void AEWMusic::EndPlay(const EEndPlayReason::Type Reason){Audio->Stop();Super::EndPlay(Reason);}
