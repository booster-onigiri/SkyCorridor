#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWDayCycle.generated.h"
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class APostProcessVolume;
class UMaterialParameterCollection;
class ACameraActor;

struct FEWDayLight
{
    double Hour=10;
    float SunPitch=0,SunYaw=0,SunLux=0,MoonLux=0,Exposure=12.6f,Day=1,Emission=1;
    FLinearColor SunColour=FLinearColor::White;
};
UCLASS()
class ENDLESSWORLD_API AEWDayCycle:public AActor
{
    GENERATED_BODY()
public:
    AEWDayCycle();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    static double HourAt(double ElapsedSeconds);
    static FEWDayLight Evaluate(double Hour,bool Legacy=false);
    float EmissionScale() const{return State.Emission;}
    float InteriorLightScale() const{return State.Emission;}
    float CameraExposureEV() const{return EyeExposure;}
    float StreetLightScale() const{return State.Emission*(1-State.Day);}
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Lighting Preview") float SkylightLeak=.16f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Lighting Preview") float AmbientLiftLux=25.f;
    void SetAuditHour(double Hour);
    // A bounded in-editor preview, exposed through the native project MCP toolset.
    UFUNCTION(BlueprintCallable, Category="Lighting Preview")
    bool PreviewLighting(double Hour,int32 View,float FillLux,float SkyFloor,float ExposureBias);
    UFUNCTION(BlueprintCallable, Category="Lighting Preview")
    FString LightingPreviewEvidence() const;
    // Exercise the ordinary clock/update path, including resume-sized jumps.
    UFUNCTION(BlueprintCallable, Category="Lighting Preview")
    bool PreviewClock(double Hour);
    TSharedRef<class FJsonObject> Evidence() const;
private:
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> Moon;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> NightFill;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> NightBounce;
    UPROPERTY() TObjectPtr<USkyLightComponent> Sky;
    UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> Fog;
    UPROPERTY() TObjectPtr<APostProcessVolume> Look;
    UPROPERTY() TObjectPtr<UMaterialParameterCollection> Parameters;
    UPROPERTY() TObjectPtr<ACameraActor> PreviewCamera;
    float StreetKeyLux=40.f,NightSkyFloor=.4f,PreviewExposureBias=0.f;
    double Started=0,ElapsedAtStart=0,AuditHour=-1;
    float EyeExposure=12.6f;
    FEWDayLight State;
    bool bLegacyHighlightAudit=false;
};
