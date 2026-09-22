#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWSkyTheatrePlan.h"
#include "EWMediaScreen.generated.h"
class FEWBrowserSurface;
class UWidgetComponent;
class UAudioComponent;
class UEWBrowserAudioWave;
class USoundSubmix;
class UInstancedStaticMeshComponent;
class FEWMediaOutputProbe;
class UPointLightComponent;
class UEWTerminalScreen;
class SEWMediaTabletView;
class SWidget;
class ACameraActor;

UCLASS()
class ENDLESSWORLD_API AEWMediaScreen:public AActor
{
    GENERATED_BODY()
public:
    AEWMediaScreen();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool bCinema=false;
    bool bSkyTheatre=false;
    bool IsTheatre() const{return bCinema || bSkyTheatre;}
    double ScreenWidth() const{return bSkyTheatre?EWSkyTheatrePlan::Width:bCinema?1850:1600;}
    bool Nearby() const;
    bool ListenerInside() const;
    bool AudibleAtListener() const;
    int32 SeatNearby() const;
    bool SitNearest();
    bool SitSeat(int32 Index);
    FTransform RoomFrame() const{return Auditorium;}
    FString Title() const{return bSkyTheatre?TEXT("天空シアター"):bCinema?TEXT("水鏡の映写室"):TEXT("広場のモニター");}
    bool Available() const;
    void OpenControls();
    void CloseControls();
    bool TabletOpen() const{return bTabletOpen;}
    bool CanControlPlayback() const;
    void ControlPlayback(FName Action);
    void ToggleLocalMute();
    TSharedPtr<SWidget> TabletFocusWidget() const;
    void FocusTablet();
    UFUNCTION(BlueprintCallable) FString TabletPreview(const FString& Action);
    UFUNCTION(BlueprintCallable) bool TabletTap(float X,float Y);
    UFUNCTION(BlueprintCallable) void CaptureTablet(const FString& Path) const;
    void LookAtScreen();
    UFUNCTION(BlueprintCallable) FString PreviewScreen(bool Film,int32 Seat);
    void CaptureSurface(const FString& Path) const;
    void CaptureBackSurface(const FString& Path) const;
    void Search(const FString& Query);
    void Stop();
    void SetVolume(float Value);
    float Volume() const{return UserVolume;}
    TSharedPtr<FEWBrowserSurface> Browser() const{return Surface;}
    TSharedRef<class FJsonObject> Evidence() const;
    UAudioComponent* Sound() const{return Audio;}
    USoundSubmix* OutputSubmix() const{return Submix;}
private:
    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<UWidgetComponent> Panel;
    UPROPERTY() TObjectPtr<UWidgetComponent> BackPanel;
    UPROPERTY() TObjectPtr<USceneComponent> TabletAnchor;
    UPROPERTY() TObjectPtr<UEWTerminalScreen> Tablet;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> TabletFrame;
    UPROPERTY() TObjectPtr<ACameraActor> TabletCamera;
    UPROPERTY() TWeakObjectPtr<AActor> PreviousView;
    TSharedPtr<SEWMediaTabletView> TabletView;
    FRotator PreviousLook=FRotator::ZeroRotator;
    void UpdateTablet();
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Frame;
    UPROPERTY() TObjectPtr<UAudioComponent> Audio;
    UPROPERTY() TObjectPtr<UAudioComponent> RightAudio;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> CinemaLights;
    UPROPERTY() TObjectPtr<UEWBrowserAudioWave> Wave;
    UPROPERTY() TObjectPtr<UEWBrowserAudioWave> RightWave;
    UPROPERTY() TObjectPtr<USoundSubmix> Submix;
    TSharedPtr<FEWBrowserSurface> Surface;
    TSharedPtr<FEWMediaOutputProbe,ESPMode::ThreadSafe> OutputProbe,MainProbe;
    FVector Station=FVector::ZeroVector;
    FTransform Auditorium=FTransform::Identity;
    float RoomGain=0;
    float UserVolume=.85f;
    float LastAudibleVolume=.85f;
    bool bTabletOpen=false;
    bool bInDistrict=false,bSoundActive=false;
};
