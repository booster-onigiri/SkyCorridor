#include "EWResidence.h"
#include "EWLocalization.h"
#include "EWCharacter.h"
#include "EWGameInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "EWDayCycle.h"
#include "TimerManager.h"
#include "Components/StaticMeshComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

namespace
{
constexpr int32 SampleRate=24000;
TArray<uint8> WaterPCM(bool Close)
{
    const int32 N=SampleRate*6;
    TArray<float> Samples;Samples.SetNumZeroed(N);
    FRandomStream Random(Close?7419:9137);
    float Soft=0,Body=0;
    for(int32 I=0;I<N;++I)
    {
        const float Noise=Random.FRandRange(-1.f,1.f);
        Soft=Soft*.84f+Noise*.16f;Body=Body*.975f+Noise*.025f;
        Samples[I]=(Soft-Body)*(Close?.12f:.045f)+Body*.13f;
    }
    for(int32 Start=0;Start<N;Start+=Random.RandRange(Close?750:3200,Close?4000:8500))
    {
        const float Frequency=Random.FRandRange(430.f,Close?1650.f:1050.f);
        const float Amplitude=Random.FRandRange(.025f,Close?.09f:.14f);
        for(int32 J=0;J<4000 && Start+J<N;++J)
        {
            const double T=double(J)/SampleRate;
            Samples[Start+J]+=Amplitude*FMath::Exp(-T*(Close?25.:36.))*
                FMath::Sin(UE_DOUBLE_TWO_PI*(Frequency*T+450.*T*T))*(1.-FMath::Exp(-T*1300.));
        }
    }
    TArray<uint8> PCM;PCM.SetNumUninitialized(N*2);
    for(int32 I=0;I<N;++I)
    {
        const float Edge=FMath::Min(1.f,FMath::Min(float(I),float(N-1-I))/480.f);
        const int16 V=int16(FMath::Clamp(Samples[I]*Edge,-.7f,.7f)*32767.f);
        PCM[I*2]=uint8(uint16(V)&255);PCM[I*2+1]=uint8(uint16(V)>>8);
    }
    return PCM;
}
TArray<uint8> MechanismPCM()
{
    const int32 N=SampleRate/5;TArray<uint8> PCM;PCM.SetNumUninitialized(N*2);
    FRandomStream Random(1687);
    for(int32 I=0;I<N;++I)
    {
        const double T=double(I)/SampleRate;
        const double A=(FMath::Sin(T*UE_DOUBLE_TWO_PI*720.)*.16+Random.FRandRange(-.06f,.06f))*
            FMath::Exp(-T*38.)*(1.-FMath::Exp(-T*1600.));
        const int16 V=int16(A*32767.);PCM[I*2]=uint8(uint16(V)&255);PCM[I*2+1]=uint8(uint16(V)>>8);
    }
    return PCM;
}
}

AEWResidence::AEWResidence()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    Root=CreateDefaultSubobject<USceneComponent>(TEXT("ResidenceRoot"));SetRootComponent(Root);
    Frame=CreateDefaultSubobject<USceneComponent>(TEXT("RoomFrame"));Frame->SetupAttachment(Root);
    HandlePivot=CreateDefaultSubobject<USceneComponent>(TEXT("ValvePivot"));HandlePivot->SetupAttachment(Frame);
}
void AEWResidence::AppendAssetPaths(TArray<FSoftObjectPath>& Paths)
{
    for(const TCHAR* Name:{TEXT("RainValveHandle"),TEXT("RainWaterCommon"),TEXT("RainWaterEaves"),TEXT("RainWaterWindow")})
        Paths.AddUnique(FSoftObjectPath(FString::Printf(TEXT("/Game/EndlessWorld/Kit/SM_%s.SM_%s"),Name,Name)));
}
bool AEWResidence::Initialize(const EW::ResidenceSpec& InSpec,const FString& InWorldCode)
{
    Spec=InSpec;WorldCode=InWorldCode;Frame->SetRelativeTransform(Spec.Frame);
    HandlePivot->SetRelativeLocation(FVector(-225,-2040,115));
    auto Mesh=[&](const TCHAR* Name,USceneComponent* Parent)->UStaticMeshComponent*
    {
        const FString Path=FString::Printf(TEXT("/Game/EndlessWorld/Kit/SM_%s.SM_%s"),Name,Name);
        auto* Asset=Cast<UStaticMesh>(FSoftObjectPath(Path).ResolveObject());
        if(!Asset){UE_LOG(LogTemp,Error,TEXT("EW_RESIDENCE_MISSING_ASSET %s"),*Path);return nullptr;}
        auto* M=NewObject<UStaticMeshComponent>(this);M->SetupAttachment(Parent);
        M->SetMobility(EComponentMobility::Movable);M->SetStaticMesh(Asset);M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        M->SetCanEverAffectNavigation(false);M->RegisterComponent();return M;
    };
    auto* Handle=Mesh(TEXT("RainValveHandle"),HandlePivot);
    auto* Common=Mesh(TEXT("RainWaterCommon"),Frame);
    EavesWater=Mesh(TEXT("RainWaterEaves"),Frame);WindowWater=Mesh(TEXT("RainWaterWindow"),Frame);
    if(!Handle || !Common || !EavesWater || !WindowWater)return false;
    auto* GI=GetGameInstance<UEWGameInstance>();
    bStateReady=GI && GI->Store() && GI->Store()->LoadResidenceState(WorldCode,Spec.Id,Spec.Revision,Mode);
    if(!bStateReady)
    {
        StateError=GI && GI->Store()?GI->Store()->Error():EWL::Pick(TEXT("窓辺の保存場所を開けません。水の選択は変更していません。"), TEXT("Cannot open the window's save folder. The water setting has not changed."));
        if(GI)GI->Notify(StateError,15);
    }
    EavesSound=CreateWaterSound(FVector(-225,-2490,-30),false);
    WindowSound=CreateWaterSound(FVector(755,-1730,100),true);
    HandleWave=NewObject<USoundWaveProcedural>(this);HandleWave->SetSampleRate(SampleRate);HandleWave->NumChannels=1;
    HandleWave->Duration=.2f;SoundWaves.Add(HandleWave);ClickPCM=MechanismPCM();
    HandleSound=NewObject<UAudioComponent>(this);HandleSound->SetupAttachment(HandlePivot);
    HandleSound->bAutoDestroy=false;HandleSound->SetAutoActivate(false);HandleSound->bOverrideAttenuation=true;
    HandleSound->AttenuationOverrides.bAttenuate=true;HandleSound->AttenuationOverrides.bSpatialize=true;
    HandleSound->AttenuationOverrides.AttenuationShapeExtents=FVector(80,0,0);
    HandleSound->AttenuationOverrides.FalloffDistance=700;HandleSound->SetSound(HandleWave);HandleSound->RegisterComponent();
    auto Lamp=[&](FVector Position,float Lumens,float Radius)
    {
        auto* Light=NewObject<UPointLightComponent>(this);Light->SetupAttachment(Frame);
        Light->SetMobility(EComponentMobility::Movable);Light->SetRelativeLocation(Position);
        // The city's daylight exposure range needs an explicit photometric
        // intensity. Legacy 4500 Unitless was only about 90 lumens.
        Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetIntensity(Lumens);
        Light->SetLightColor(FLinearColor(1.f,.62f,.30f));Light->SetAttenuationRadius(Radius);
        Light->SetSourceRadius(20.f);Light->SetVolumetricScatteringIntensity(0.f);
        Light->SetCastShadows(false);Light->RegisterComponent();RoomLights.Add(Light);
    };
    Lamp(FVector(340,-1350,240),85000.f,550.f);
    Lamp(FVector(225,-1900,260),30000.f,450.f);
    GetWorldTimerManager().SetTimer(LightingTimer,this,&AEWResidence::UpdateLighting,.25f,true);
    ApplyState(false);
    UE_LOG(LogTemp,Display,TEXT("EW_RESIDENCE_READY id=%s mode=%d state=%d"),*Spec.Id,Mode,int32(bStateReady));
    return true;
}
UAudioComponent* AEWResidence::CreateWaterSound(FVector Position,bool Close)
{
    auto Data=MakeShared<TArray<uint8>,ESPMode::ThreadSafe>(WaterPCM(Close));
    auto* Wave=NewObject<USoundWaveProcedural>(this);Wave->SetSampleRate(SampleRate);Wave->NumChannels=1;
    Wave->Duration=INDEFINITELY_LOOPING_DURATION;Wave->VirtualizationMode=EVirtualizationMode::PlayWhenSilent;
    Wave->OnSoundWaveProceduralUnderflow.BindLambda([Data](USoundWaveProcedural* Source,int32 Samples)
    {
        const int32 Repeats=FMath::Max(1,FMath::DivideAndRoundUp(Samples*2,Data->Num()));
        for(int32 I=0;I<Repeats;++I)Source->QueueAudio(Data->GetData(),Data->Num());
    });
    Wave->QueueAudio(Data->GetData(),Data->Num());SoundWaves.Add(Wave);
    auto* A=NewObject<UAudioComponent>(this);A->SetupAttachment(Frame);A->SetRelativeLocation(Position);
    A->SetAutoActivate(false);A->bAutoDestroy=false;A->bOverrideAttenuation=true;
    A->AttenuationOverrides.bAttenuate=true;A->AttenuationOverrides.bSpatialize=true;
    A->AttenuationOverrides.AttenuationShapeExtents=FVector(150,0,0);A->AttenuationOverrides.FalloffDistance=2400;
    A->AttenuationOverrides.bEnableOcclusion=true;A->AttenuationOverrides.OcclusionLowPassFilterFrequency=1300;
    A->AttenuationOverrides.OcclusionVolumeAttenuation=.35f;A->AttenuationOverrides.OcclusionInterpolationTime=.3f;
    A->SetSound(Wave);A->SetVolumeMultiplier(.01f);A->RegisterComponent();A->Play();return A;
}
void AEWResidence::ApplyState(bool Animate)
{
    EavesWater->SetVisibility(Mode==0);WindowWater->SetVisibility(Mode==1);
    TargetAngle=Mode==0?-42.:42.;
    if(!Animate){HandleAngle=TargetAngle;HandlePivot->SetRelativeRotation(FRotator(HandleAngle,0,0));}
    const float Duration=Animate?.45f:0.f;
    if(EavesSound)EavesSound->AdjustVolume(Duration,Mode==0?.9f:.015f);
    if(WindowSound)WindowSound->AdjustVolume(Duration,Mode==1?.8f:.015f);
    if(Animate)
    {
        HandleSound->Stop();HandleWave->ResetAudio();HandleWave->QueueAudio(ClickPCM.GetData(),ClickPCM.Num());
        HandleSound->Play();ClickRemaining=.2;SetActorTickEnabled(true);
    }
}
EEWResidenceAction AEWResidence::Nearby(const AEWCharacter* P,double& DistanceSquared) const
{
    DistanceSquared=DBL_MAX;if(!P || !P->Camera || P->IsLiftRiding())return EEWResidenceAction::None;
    const FVector Eye=P->Camera->GetComponentLocation(),Forward=P->Camera->GetForwardVector();
    EEWResidenceAction Best=EEWResidenceAction::None;
    auto Try=[&](EEWResidenceAction Action,FVector Local,double Radius)
    {
        const FVector Aim=GetActorTransform().TransformPosition(Local);
        const double D=FVector::DistSquared(P->GetActorLocation(),Aim);
        if(D>Radius*Radius || D>=DistanceSquared || FVector::DotProduct((Aim-Eye).GetSafeNormal(),Forward)<.3)return;
        FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(ResidenceInteraction),false,P);Params.AddIgnoredActor(this);
        if(GetWorld()->LineTraceSingleByChannel(Hit,Eye,Aim,ECC_Visibility,Params) && FVector::Dist(Hit.ImpactPoint,Aim)>85.)return;
        DistanceSquared=D;Best=Action;
    };
    Try(EEWResidenceAction::Valve,Spec.Valve,250.);
    Try(EEWResidenceAction::Seat,Spec.Frame.TransformPosition(FVector(650,-1710,115)),260.);
    return Best;
}
FString AEWResidence::Hint(EEWResidenceAction Action) const
{
    if(Action==EEWResidenceAction::Seat)return EWL::Pick(TEXT("雨継ぎの窓辺　座って街を眺める"), TEXT("Raincatcher Window — Sit and watch the city"));
    if(Action==EEWResidenceAction::Valve)return Mode==0?EWL::Pick(TEXT("分水器　水を窓辺へ流す"), TEXT("Water valve — Direct water to the window")):EWL::Pick(TEXT("分水器　水を外樋へ戻す"), TEXT("Water valve — Return water to the outer gutter"));
    return {};
}
bool AEWResidence::ToggleWater(FString& Message)
{
    auto* GI=GetGameInstance<UEWGameInstance>();
    if(!GI || !GI->Store()){Message=EWL::Pick(TEXT("水の選択を保存できません。以前の流れを保っています。"), TEXT("Cannot save the water setting. Keeping the previous flow."));return false;}
    if(!GI->Store()->SaveResidenceState(WorldCode,Spec.Id,Spec.Revision,1-Mode))
    {Message=GI->Store()->Error();return false;}
    Mode=1-Mode;bStateReady=true;StateError.Empty();ApplyState(true);
    Message=Mode==1?EWL::Pick(TEXT("水が窓辺へ流れ始めました。この流れを覚えておきます。"), TEXT("Water is flowing by the window. This setting will be remembered.")):EWL::Pick(TEXT("水を外樋へ戻しました。この流れを覚えておきます。"), TEXT("Water returned to the outer gutter. This setting will be remembered."));
    UE_LOG(LogTemp,Display,TEXT("EW_RESIDENCE_FLOW id=%s mode=%d saved=1"),*Spec.Id,Mode);return true;
}
FTransform AEWResidence::SeatTransform() const
{return FTransform(FRotator(Spec.ViewPitch,Spec.ViewYaw,0),GetActorTransform().TransformPosition(Spec.Seat));}
FTransform AEWResidence::StandTransform() const
{return FTransform(FRotator(0,Spec.ViewYaw,0),GetActorTransform().TransformPosition(Spec.Stand));}
void AEWResidence::Tick(float Delta)
{
    Super::Tick(Delta);HandleAngle=FMath::FInterpConstantTo(HandleAngle,TargetAngle,double(Delta),260.);
    HandlePivot->SetRelativeRotation(FRotator(HandleAngle,0,0));ClickRemaining-=Delta;
    if(ClickRemaining<=0 && HandleSound)HandleSound->Stop();
    if(FMath::Abs(HandleAngle-TargetAngle)<.01 && ClickRemaining<=0)SetActorTickEnabled(false);
}
void AEWResidence::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(LightingTimer);
    for(auto* A:{EavesSound.Get(),WindowSound.Get(),HandleSound.Get()})if(A)A->Stop();
    // Audio callbacks own only their immutable PCM buffer, never this actor.
    // Their wave can safely finish on the audio thread after this actor retires.
    Super::EndPlay(Reason);
}
void AEWResidence::UpdateLighting()
{
    const auto* G=GetGameInstance<UEWGameInstance>();const float Scale=G && G->DayCycle?G->DayCycle->InteriorLightScale():1;
    for(int32 I=0;I<RoomLights.Num();++I)RoomLights[I]->SetIntensity((I==0?85000.f:30000.f)*Scale);
}
