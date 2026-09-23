#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/WidgetComponent.h"
#include "EWMemoryPlan.h"
#include "EWSaveStore.h"
#include "EWTerminal.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UAudioComponent;
class UEWBrowserAudioWave;
class FEWBrowserSurface;
class SEWTerminalView;

UCLASS()
class ENDLESSWORLD_API UEWTerminalScreen : public UWidgetComponent
{
    GENERATED_BODY()
public:
    UEWTerminalScreen();
    void BindToViewport();
};

UCLASS()
class ENDLESSWORLD_API AEWTerminal : public AActor
{
    GENERATED_BODY()
public:
    void RefreshLanguage() { Refresh(); }
    AEWTerminal();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintCallable) void Open();
    UFUNCTION(BlueprintCallable) void Close();
    UFUNCTION(BlueprintCallable) void ToggleObservation();
    UFUNCTION(BlueprintCallable) bool RecordNearby();
    UFUNCTION(BlueprintCallable) void ShowPage(int32 Page);
    UFUNCTION(BlueprintCallable) FString TerminalEvidence() const;
    void SelectTarget(int32 Index);
    void SearchVideo(const FString& Query);
    void StopVideo();
    void PauseVideo();
    void InviteFriends();
    bool MediaAudible() const;
    bool Observing() const {return bObserving;}
    bool Recorded(int32 Index) const {return Found.Contains(Index);}
    int32 RecordCount() const {return Found.Num();}
    int32 TargetIndex() const {return Target;}
    int32 PageIndex() const {return Page;}
    double ObservedSecondsForAudit() const {return ObservedSeconds;}
    const TArray<FEWMemoryPlace>& Places() const {return Memories;}
    FString Hint() const;
    FString RouteText(int32 Index) const;
    TSharedPtr<FEWBrowserSurface> Browser() const {return Surface;}
    TSharedRef<class FJsonObject> Evidence() const;
    TSharedPtr<SWidget> FocusWidget() const;
    void FocusFirst();
    UFUNCTION(BlueprintCallable) void CaptureScreen(const FString& Path) const;
    UFUNCTION(BlueprintCallable) bool PreviewLandmark(int32 Index,int32 PreviewView,float Hour);
    UFUNCTION(BlueprintCallable) FString Sky92FogEvidence() const;
    UFUNCTION(BlueprintCallable) bool PreviewAt(int32 Index); // explicit editor/audit fixture only
    UFUNCTION(BlueprintCallable) bool PreviewTap(float X,float Y); // Slate pointer routing in isolated tests
    UFUNCTION(BlueprintCallable) bool PreviewTrace(float Strength,float Opacity,float Hour);
private:
    UPROPERTY() TObjectPtr<class ACameraActor> PreviewCamera;
    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY() TObjectPtr<UEWTerminalScreen> Screen;
    UPROPERTY() TObjectPtr<UPostProcessComponent> Observation;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Shadows;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> ShadowMaterials;
    UPROPERTY() TObjectPtr<UAudioComponent> Audio;
    UPROPERTY() TObjectPtr<UEWBrowserAudioWave> Wave;
    TSharedPtr<SEWTerminalView> View;
    TSharedPtr<FEWBrowserSurface> Surface;
    TUniquePtr<FEWSaveStore> Journal;
    TArray<FEWMemoryPlace> Memories;
    TSet<int32> Found;
    int32 Page=0,Target=0,Near=INDEX_NONE,LastObserved=INDEX_NONE;
    float Raised=0,Scan=0;
    float TraceStrength=3.5f,TraceOpacity=.38f;
    double ObservedSeconds=0;
    bool bObserving=false,bSilent=false,bSoundPlaying=false,bHadOpen=false,bIntroShown=false;
    void BuildPlaces();
    void Refresh();
};
