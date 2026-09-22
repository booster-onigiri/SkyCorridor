#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWPhotoMode.generated.h"
class ACameraActor;
class AEWCinemaPeer;
class UWidgetComponent;

namespace EWPhoto
{
    // The camera letterboxes to the requested ratio; export exactly that central frame.
    FIntRect Crop(int32 Width,int32 Height,bool Portrait);
    FColor ToSDR(const FLinearColor& Pixel);
}
UCLASS()
class ENDLESSWORLD_API AEWPhotoMode : public AActor
{
    GENERATED_BODY()
public:
    AEWPhotoMode();
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void Open();
    void Close();
    void Shoot();
    void TogglePortrait();
    void ToggleSelfie();
    void ToggleNames();
    void CycleTimer();
    void SetFieldOfView(float Value);
    void Turn(float Degrees);
    void Tilt(float Degrees);
    bool Active() const {return bActive;}
    bool Portrait() const {return bPortrait;}
    bool Selfie() const {return bSelfie;}
    bool Names() const {return bNames;}
    float FieldOfView() const {return FOV;}
    int32 Timer() const {return TimerSeconds;}
    bool Busy() const {return CaptureAt>0 || !PendingPath.IsEmpty();}
    FString Status() const;
    FString LastFile() const {return LastPath;}
    UFUNCTION(BlueprintCallable, Category="Photo verification") FString PhotoEvidence() const;
    UFUNCTION(BlueprintCallable, Category="Photo verification") bool PhotoAction(const FString& Action);
private:
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    UPROPERTY() TObjectPtr<AEWCinemaPeer> SelfAvatar;
    TMap<TWeakObjectPtr<UWidgetComponent>,bool> NameVisibility;
    TWeakObjectPtr<AActor> PreviousView;
    FDelegateHandle CaptureHandle;
    FDelegateHandle HDRCaptureHandle;
    FString PendingPath,LastPath,Message;
    bool bActive=false,bPortrait=false,bSelfie=false,bNames=false;
    float FOV=65,CameraYaw=0,CameraPitch=0;
    int32 TimerSeconds=3;
    double CaptureAt=0,RequestedAt=0;
    void UpdateCamera();
    void RestoreNames();
    void ReceiveScreenshot(int32 Width,int32 Height,const TArray<FColor>& Pixels);
    void ReceiveHDRScreenshot(int32 Width,int32 Height,const TArray<FLinearColor>& Pixels);
};
