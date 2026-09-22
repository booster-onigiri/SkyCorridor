#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "EWCharacter.generated.h"

class UCameraComponent;
class SEWOverlay;
class AEWResidence;

UCLASS()
class ENDLESSWORLD_API AEWCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AEWCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    void SetStreamingHold(bool Hold);
    bool IsStreamingHeld() const { return bStreamingHold; }
    void ResetSafeLocation();
    void ShiftLocalOrigin(const FVector& Offset);
    void SetTestMovement(const FVector& Direction, bool Enabled);
    void ClearHeldInput();
    void SetLiftRiding(bool Riding);
    bool IsLiftRiding() const {return bLiftRiding;}
    bool SitAt(AEWResidence* Residence);
    bool SitAtTransform(AActor* SeatingActor,const FTransform& Seat,const FTransform& Stand);
    bool LeaveSeat();
    void UpdateTransitSeat(AActor* Owner,const FVector& Position,const FTransform& Stand,bool Locked);
    bool IsSeated() const { return bSeated; }
    FTransform BookmarkTransform() const;
    int32 FallRecoveries = 0;
    int32 ForwardEvents=0,StrafeEvents=0,LookEvents=0,JumpEvents=0;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
private:
    bool bStreamingHold = true, bSprint = false, bTestMovement = false, bLiftRiding = false;
    bool bSeated = false, bTransitSeatLocked = false;
    TWeakObjectPtr<AActor> SeatOwner;
    FTransform SeatStandTransform = FTransform::Identity;
    FVector LastSafe = FVector::ZeroVector, TestDirection = FVector::ZeroVector;
    TWeakObjectPtr<AActor> LastSafeMovingBase;
    FVector LastSafeOnBase=FVector::ZeroVector;
    void Forward(float Value);
    void Right(float Value);
    void Turn(float Value);
    void Look(float Value);
    void TurnPad(float Value);
    void LookPad(float Value);
    void StartJump();
    void VoiceOn();
    void VoiceOff();
    void CityMenu();
    void OpenJournal();
    void OpenTerminal();
    void OpenMenu();
    void RecordPlace();
    void SprintOn();
    void SprintOff();
    void Screenshot();
    void Photo();
    void Fullscreen();
    void Diagnostics();
    bool AcceptInput() const;
};

UCLASS()
class ENDLESSWORLD_API AEWPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual bool InputKey(const FInputKeyEventArgs& Params) override;
    void RefreshInterface();
    FString FocusedControlLabel() const;
private:
    TSharedPtr<SEWOverlay> Overlay;
};

UCLASS()
class ENDLESSWORLD_API AEWGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AEWGameMode();
    virtual void BeginPlay() override;
};
