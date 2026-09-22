#include "EWDayCycle.h"
#include "EWWorldClock.h"
#include "EWNightLighting.h"
#include "EWInteriors.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWLightingViews.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EWMediaScreen.h"
#include "EWCinemaSession.h"
#include "EWSocialSession.h"
#include "EWSaveStore.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/FileManager.h"

AEWDayCycle::AEWDayCycle()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DayCycleRoot")));
    Moon=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moonlight"));Moon->SetupAttachment(RootComponent);
    Moon->SetMobility(EComponentMobility::Movable);Moon->SetAtmosphereSunLight(true);Moon->SetAtmosphereSunLightIndex(1);
    Moon->SetLightSourceAngle(.65f);Moon->SetLightColor(FLinearColor(.40,.57,1));Moon->SetIntensity(0);
    Moon->SetCastShadows(true);
    Moon->ForwardShadingPriority=1;
    // A secondary sky direction reaches street openings, while retaining
    // the shadows of balconies, pillars and furniture.
    NightFill=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("NightStreetFill"));
    NightBounce=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("NightStreetBounce"));
    for(auto* Fill:{NightFill.Get(),NightBounce.Get()})
    {
        Fill->SetupAttachment(RootComponent);Fill->SetMobility(EComponentMobility::Movable);
        // Directional lights default to atmosphere slot 0. An auxiliary light
        // can win that slot while the sun is off and retain it as dawn grows:
        // intensity-only updates do not reselect the brightest atmosphere light.
        // Only the real sun (0) and moon (1) may illuminate the atmosphere.
        Fill->SetAtmosphereSunLight(false);
        Fill->SetCastShadows(true);Fill->SetLightColor(FLinearColor(.62,.73,.94));
        Fill->SetLightSourceAngle(3.f);
        Fill->SetVolumetricScatteringIntensity(0);Fill->SetIntensity(0);
    }
    NightFill->SetRelativeRotation(FRotator(-45,50,0));
    NightBounce->SetRelativeRotation(FRotator(-25,230,0));
    // A low diffuse baseline keeps enclosed public walkways readable. The
    // shadow-casting key remains dominant; the old unshadowed pair was 175 lux.
    NightBounce->SetCastShadows(false);
}
double AEWDayCycle::HourAt(double ElapsedSeconds)
{
    // One city day takes 30 real minutes. No dependency on FPS, time dilation
    // or the hands of the city's real-time clocks.
    return EWWorldClock::Advance(10.,ElapsedSeconds);
}
FEWDayLight AEWDayCycle::Evaluate(double Hour,bool Legacy)
{
    FEWDayLight V;V.Hour=FMath::Fmod(Hour+48.,24.);
    const float Height=FMath::Sin(float((V.Hour-6.)/24.*2.*PI));
    V.Day=FMath::SmoothStep(-.12f,.60f,Height);
    const float Bright=FMath::Pow(V.Day,.45f);
    V.SunPitch=-65.f*Height;V.SunYaw=float((V.Hour-12)*15-80);
    // Attenuate low-angle sunlight before applying exposure. The old .30
    // exponent reached daylight lux while exposure was still near twilight.
    V.SunLux=110000.f*FMath::Pow(FMath::Max(Height,0.f),Legacy?.30f:1.3f);
    V.MoonLux=80.f*(1-V.Day);
    // Sum light in linear space, then convert to EV. This keeps the ratio of
    // direct sunlight to exposure bounded at every hour, not only at noon.
    // Preserve the night anchor and compensate the existing sky/practicals
    // with Emission below so reducing glare does not extinguish room lights.
    const float NightExposure=FMath::Pow(2.f,6.3f);
    V.Exposure=Legacy?FMath::Lerp(6.3f,12.6f,Bright):
        FMath::Log2(NightExposure+(FMath::Pow(2.f,13.2f)-NightExposure)*(V.SunLux/110000.f));
    V.Emission=FMath::Pow(2.f,V.Exposure-14.2f);
    V.SunColour=FMath::Lerp(FLinearColor(1,.29,.095),FLinearColor(1,.98,.93),FMath::Clamp(Height/.60f,0.f,1.f));
    return V;
}
void AEWDayCycle::BeginPlay()
{
    Super::BeginPlay();Started=FPlatformTime::Seconds();
    for(TActorIterator<ADirectionalLight> I(GetWorld());I;++I){Sun=Cast<UDirectionalLightComponent>(I->GetLightComponent());if(Sun)break;}
    for(TActorIterator<ASkyLight> I(GetWorld());I;++I){Sky=I->GetLightComponent();break;}
    for(TActorIterator<AExponentialHeightFog> I(GetWorld());I;++I){Fog=I->GetComponent();break;}
    for(TActorIterator<APostProcessVolume> I(GetWorld());I;++I){if(I->bUnbound){Look=*I;break;}}
    if(Sun){Sun->ForwardShadingPriority=2;Sun->SetMobility(EComponentMobility::Movable);Sun->SetCastShadows(true);Sun->SetLightSourceAngle(.7f);Sun->SetAtmosphereSunLight(true);Sun->SetAtmosphereSunLightIndex(0);}
    if(Sky){Sky->SetMobility(EComponentMobility::Movable);Sky->SetRealTimeCaptureEnabled(true);Sky->bLowerHemisphereIsBlack=false;Sky->SetLowerHemisphereColor(FLinearColor(.04f,.065f,.085f));}
    Parameters=LoadObject<UMaterialParameterCollection>(nullptr,TEXT("/Game/EndlessWorld/Materials/MPC_DayCycle.MPC_DayCycle"));
    const double UtcSeconds=FDateTime::UtcNow().GetTicks()/double(ETimespan::TicksPerSecond);
    double Epoch=UtcSeconds;
    if(const auto* G=GetGameInstance<UEWGameInstance>())if(G->Store())
    {
        const FString File=G->Store()->Root()/TEXT("world-cycle.json");FString Text;TSharedPtr<FJsonObject> Json;
        bool Loaded=false;double PreviousPeriod=EWWorldClock::LegacyDaySeconds;
        if(FFileHelper::LoadFileToString(Text,*File) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Json) && Json)
        {
            double Saved=0;
            if(Json->TryGetNumberField(TEXT("epoch_utc_seconds"),Saved) && FMath::IsFinite(Saved) && Saved>0){Epoch=Saved;Loaded=true;}
            double Period=0;
            if(Json->TryGetNumberField(TEXT("day_seconds"),Period) && FMath::IsFinite(Period) && Period>=60 && Period<=86400)PreviousPeriod=Period;
        }
        const bool Migrate=Loaded && PreviousPeriod!=EWWorldClock::DaySeconds;
        if(Migrate)Epoch=EWWorldClock::RebaseEpoch(UtcSeconds,Epoch,PreviousPeriod);
        if(!Loaded || Migrate)
        {
            // Keep the previous file and replace atomically, so an interrupted
            // update cannot discard the user's saved phase.
            const FString Backup=File+TEXT(".before-30min");
            const bool BackedUp=!FPaths::FileExists(File) || FPaths::FileExists(Backup) || IFileManager::Get().Copy(*Backup,*File,false)==COPY_OK;
            auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("epoch_utc_seconds"),Epoch);O->SetNumberField(TEXT("day_seconds"),EWWorldClock::DaySeconds);
            Text.Reset();FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));
            const FString Temp=File+TEXT(".writing");
            const bool Saved=BackedUp && FFileHelper::SaveStringToFile(Text,*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
                && IFileManager::Get().Move(*File,*Temp,true,true,false,true);
            if(!Saved)UE_LOG(LogTemp,Warning,TEXT("Could not save the city day-cycle update; the previous file is retained."));
        }
    }
    ElapsedAtStart=FMath::Max(0.,UtcSeconds-Epoch);
    FParse::Value(FCommandLine::Get(),TEXT("EWDayHour="),AuditHour);
    bLegacyHighlightAudit=FParse::Param(FCommandLine::Get(),TEXT("EWHighlight84Audit")) && FParse::Param(FCommandLine::Get(),TEXT("EWHighlight84Legacy"));
    State=Evaluate(AuditHour>=0?AuditHour:HourAt(ElapsedAtStart),bLegacyHighlightAudit);EyeExposure=State.Exposure;Tick(.1f);
}
void AEWDayCycle::SetAuditHour(double Hour)
{
    if(FMath::IsFinite(Hour) && Hour>=0 && Hour<24 &&
       (GIsEditor || FParse::Param(FCommandLine::Get(),TEXT("EWPool90Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWAero87Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWCascade86Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWExplore85Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWHighlight84Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWHotel83Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWCinemaAudit"))))AuditHour=Hour;
}
bool AEWDayCycle::PreviewClock(double Hour)
{
    if(!(GIsEditor || FParse::Param(FCommandLine::Get(),TEXT("EWLighting91Audit"))) || !FMath::IsFinite(Hour) || Hour<0 || Hour>=24)return false;
    AuditHour=-1;ElapsedAtStart=FMath::Fmod(Hour-10.+24.,24.)/24.*EWWorldClock::DaySeconds;
    Started=FPlatformTime::Seconds();return true;
}
bool AEWDayCycle::PreviewLighting(double Hour,int32 View,float FillLux,float SkyFloor,float ExposureBias)
{
#if WITH_EDITOR
    if(!GIsEditor || !FMath::IsFinite(Hour) || !FMath::IsFinite(FillLux) || !FMath::IsFinite(SkyFloor) || !FMath::IsFinite(ExposureBias))return false;
    auto* G=GetGameInstance<UEWGameInstance>();auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !G->Manager || !PC)return false;
    const auto R=G->Manager->RecipeAt({0,0});if(!R)return false;
    AuditHour=Hour<0?-1:FMath::Fmod(Hour,24.);StreetKeyLux=FMath::Clamp(FillLux,0.f,80.f);
    NightSkyFloor=FMath::Clamp(SkyFloor,0.f,.5f);PreviewExposureBias=FMath::Clamp(ExposureBias,-2.f,2.f);
    if(Hour<0){PC->SetViewTarget(PC->GetPawn());return true;}
    const auto CameraPose=EWLighting::PreviewView(*R,View);
    if(!PreviewCamera)PreviewCamera=GetWorld()->SpawnActor<ACameraActor>();
    PreviewCamera->SetActorLocationAndRotation(G->Manager->ToRender({0,0},CameraPose.GetLocation()),CameraPose.Rotator());
    PreviewCamera->GetCameraComponent()->FieldOfView=85;PC->SetViewTarget(PreviewCamera);G->SetMenu(EEWMenu::None);
    return true;
#else
    return false;
#endif
}
FString AEWDayCycle::LightingPreviewEvidence() const
{FString Text;FJsonSerializer::Serialize(Evidence(),TJsonWriterFactory<>::Create(&Text));return Text;}
void AEWDayCycle::Tick(float Delta)
{
    Super::Tick(Delta);
    const auto* SessionGI=GetGameInstance<UEWGameInstance>();
    const double Hour=SessionGI && SessionGI->SocialSession && SessionGI->SocialSession->HasSharedHour()?SessionGI->SocialSession->SharedHour():SessionGI && SessionGI->CinemaSession && SessionGI->CinemaSession->HasSharedHour()?SessionGI->CinemaSession->SharedHour():HourAt(ElapsedAtStart+FPlatformTime::Seconds()-Started);
    State=Evaluate(AuditHour>=0?AuditHour:Hour,bLegacyHighlightAudit);
    State.Exposure+=PreviewExposureBias;
    State.Emission=FMath::Pow(2.f,State.Exposure-14.2f);
    if(Sun){Sun->SetWorldRotation(FRotator(State.SunPitch,State.SunYaw,0));Sun->SetIntensity(State.SunLux);Sun->SetLightColor(State.SunColour);}
    Moon->SetWorldRotation(FRotator(-State.SunPitch,State.SunYaw+180,0));Moon->SetIntensity(State.MoonLux);

    if(Fog)
    {
        Fog->SetFogDensity(FMath::Lerp(.005f,.011f,State.Day));
        Fog->SetFogInscatteringColor(FMath::Lerp(FLinearColor(.021,.036,.080),FLinearColor(.29,.47,.66),State.Day));
    }
    if(Parameters)if(auto* Instance=GetWorld()->GetParameterCollectionInstance(Parameters))
    {
        Instance->SetScalarParameterValue(TEXT("CityEmission"),State.Emission);
        Instance->SetScalarParameterValue(TEXT("CityNightEmission"),State.Emission*(1-State.Day));
    }
    const auto* G=GetGameInstance<UEWGameInstance>();
    const bool Cinema=G && G->CinemaScreen && G->CinemaScreen->ListenerInside();
    // The key respects openings and architecture. A much weaker diffuse
    // baseline retains floor detail below the deep stacked balconies.
    const float StreetFillScale=(1-State.Day)*FMath::Pow(2.f,State.Exposure-6.8f);
    NightFill->SetIntensity(Cinema?0:StreetKeyLux*StreetFillScale);
    NightBounce->SetIntensity(Cinema?0:AmbientLiftLux*StreetFillScale);
    // The unbaked modular shell receives a strong dynamic sky fill. Reduce
    // that fill for the enclosed auditorium while its practical lights remain
    // photometric; exterior views keep the full time-of-day sky capture.
    // The exposure-scaled sky alone drops below .001 at midnight and loses
    // detail in the stacked walkways. Keep a soft moonlit ambient floor; it
    // fades quadratically so the existing daylight exposure stays balanced.
    const float AmbientFloor=NightSkyFloor*FMath::Square(1-State.Day);
    // Low-angle direct light is attenuated more strongly now. Restore a
    // bounded amount of diffuse sky in the deep streets, rather than raising
    // the camera exposure and bleaching the sunlit rooms again. Fade at the
    // horizon; the midnight sky floor and practical lighting stay unchanged.
    const float SolarHeight=FMath::Max(0.f,-State.SunPitch/65.f);
    const float SkyBalance=bLegacyHighlightAudit?1.f:FMath::Lerp(1.f,
        FMath::Pow(FMath::Max(.2f,SolarHeight),-.85f),FMath::SmoothStep(0.f,.2f,SolarHeight));
    const float ExteriorSky=FMath::Max(AmbientFloor,FMath::Lerp(.16f,2.2f,State.Day)*State.Emission*SkyBalance);
    if(Sky)Sky->SetIntensity(Cinema?.008f:ExteriorSky);
    float TargetExposure=Cinema?7.5f:State.Exposure;
    if(!Cinema && G && G->CinemaScreen && G->CinemaScreen->Available())
    {
        if(const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))
        {
            const FVector P=G->CinemaScreen->RoomFrame().InverseTransformPosition(Camera->GetCameraLocation());
            if(P.X>-1340 && P.X<1340 && P.Y>-2200 && P.Y<-1100 && P.Z>0 && P.Z<420)
                TargetExposure=FMath::Min(State.Exposure,10.5f);
        }
    }
    EyeExposure=FMath::FInterpTo(EyeExposure,TargetExposure,Delta,2.0f);
    if(Look)
    {
        // Edge-aware local exposure retains sunlit stone detail without
        // removing the shadows beneath galleries. Night and cinema keep
        // their authored contrast. Detail strength 1 avoids sharpening halos.
        if(!bLegacyHighlightAudit)
        {
            const float Amount=Cinema?0.f:State.Day;
            auto& S=Look->Settings;
            S.bOverride_LocalExposureHighlightContrastScale=true;S.LocalExposureHighlightContrastScale=FMath::Lerp(1.f,.75f,Amount);
            S.bOverride_LocalExposureShadowContrastScale=true;S.LocalExposureShadowContrastScale=FMath::Lerp(1.f,.8f,Amount);
            S.bOverride_LocalExposureDetailStrength=true;S.LocalExposureDetailStrength=1.f;
            S.bOverride_LocalExposureBlurredLuminanceBlend=true;S.LocalExposureBlurredLuminanceBlend=.4f;
        }
        Look->Settings.bOverride_LumenSkylightLeaking=true;Look->Settings.LumenSkylightLeaking=SkylightLeak;
        Look->Settings.bOverride_LumenFullSkylightLeakingDistance=true;Look->Settings.LumenFullSkylightLeakingDistance=900.f;
        Look->Settings.bOverride_LumenDiffuseColorBoost=true;Look->Settings.LumenDiffuseColorBoost=1.1f;
        Look->Settings.bOverride_IndirectLightingIntensity=true;Look->Settings.IndirectLightingIntensity=1.f;
        Look->Settings.bOverride_AmbientOcclusionIntensity=true;Look->Settings.AmbientOcclusionIntensity=.7f;
        // Fast audit jumps span hours in one frame. Let Lumen catch up before
        // comparing them; ordinary play advances gradually over 30 real minutes.
        if(FParse::Param(FCommandLine::Get(),TEXT("EWAero87Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWCascade86Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWExplore85Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWHighlight84Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWCinemaAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWHotel83Audit")) || (GIsEditor && AuditHour>=0))
        {
            Look->Settings.bOverride_LumenSceneLightingUpdateSpeed=true;
            Look->Settings.bOverride_LumenFinalGatherLightingUpdateSpeed=true;
            Look->Settings.LumenSceneLightingUpdateSpeed=4;
            Look->Settings.LumenFinalGatherLightingUpdateSpeed=4;
        }
        Look->Settings.bOverride_AutoExposureMinBrightness=true;Look->Settings.bOverride_AutoExposureMaxBrightness=true;
        Look->Settings.AutoExposureMinBrightness=EyeExposure;Look->Settings.AutoExposureMaxBrightness=EyeExposure;
    }
}
TSharedRef<FJsonObject> AEWDayCycle::Evidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("game_hour"),State.Hour);O->SetNumberField(TEXT("day_seconds"),EWWorldClock::DaySeconds);
    for(TActorIterator<AEWNightLighting> It(GetWorld());It;++It)O->SetObjectField(TEXT("night_fixtures"),It->Evidence());
    for(TActorIterator<AEWInteriorLighting> It(GetWorld());It;++It)O->SetObjectField(TEXT("interior_fixtures"),It->Evidence());
    O->SetNumberField(TEXT("sun_lux"),State.SunLux);O->SetNumberField(TEXT("moon_lux"),State.MoonLux);
    O->SetNumberField(TEXT("sun_pitch"),State.SunPitch);O->SetNumberField(TEXT("exposure_ev"),EyeExposure);
    O->SetNumberField(TEXT("emission_scale"),State.Emission);O->SetBoolField(TEXT("parameter_collection"),Parameters!=nullptr);
    O->SetBoolField(TEXT("sun_bound"),Sun!=nullptr);O->SetBoolField(TEXT("sky_bound"),Sky!=nullptr);
    O->SetBoolField(TEXT("fog_bound"),Fog!=nullptr);O->SetBoolField(TEXT("exposure_bound"),Look!=nullptr);
    O->SetNumberField(TEXT("sky_intensity"),Sky?Sky->Intensity:0);
    O->SetNumberField(TEXT("night_fill_lux"),NightFill?NightFill->Intensity:0);
    O->SetNumberField(TEXT("night_bounce_lux"),NightBounce?NightBounce->Intensity:0);
    O->SetBoolField(TEXT("street_key_casts_shadows"),NightFill && NightFill->CastShadows);
    O->SetBoolField(TEXT("auxiliary_atmosphere_disabled"),!NightFill->bAtmosphereSunLight && !NightBounce->bAtmosphereSunLight);
    O->SetBoolField(TEXT("sun_atmosphere_slot_valid"),Sun && Sun->bAtmosphereSunLight && Sun->AtmosphereSunLightIndex==0);
    O->SetBoolField(TEXT("moon_atmosphere_slot_valid"),Moon && Moon->bAtmosphereSunLight && Moon->AtmosphereSunLightIndex==1);
    O->SetNumberField(TEXT("skylight_leaking"),Look?Look->Settings.LumenSkylightLeaking:0);
    O->SetBoolField(TEXT("legacy_highlight_audit"),bLegacyHighlightAudit);
    if(const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))
    {
        const auto& PP=Camera->GetCameraCacheView().PostProcessSettings;
        O->SetNumberField(TEXT("camera_pp_weight"),Camera->GetCameraCacheView().PostProcessBlendWeight);
        O->SetNumberField(TEXT("camera_exposure_bias"),PP.AutoExposureBias);
    }
    if(Look)
    {
        O->SetNumberField(TEXT("exposure_compensation"),Look->Settings.AutoExposureBias);
        O->SetNumberField(TEXT("local_highlight_contrast"),Look->Settings.LocalExposureHighlightContrastScale);
        O->SetNumberField(TEXT("local_shadow_contrast"),Look->Settings.LocalExposureShadowContrastScale);
        O->SetNumberField(TEXT("bloom_intensity"),Look->Settings.BloomIntensity);
        O->SetNumberField(TEXT("film_shoulder"),Look->Settings.FilmShoulder);
        O->SetNumberField(TEXT("lumen_scene_update_speed"),Look->Settings.LumenSceneLightingUpdateSpeed);
        O->SetNumberField(TEXT("lumen_gather_update_speed"),Look->Settings.LumenFinalGatherLightingUpdateSpeed);
    }
    O->SetBoolField(TEXT("audit_override"),AuditHour>=0);O->SetBoolField(TEXT("wall_clocks_use_real_time"),true);return O;
}
