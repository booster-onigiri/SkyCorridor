#include "EWCharacter.h"
#include "EWTerminal.h"
#include "EWMediaScreen.h"
#include "EWGameInstance.h"
#include "EWPhotoMode.h"
#include "EWSocialSession.h"
#include "EWChunkManager.h"
#include "EWAeroYacht.h"
#include "EWInterface.h"
#include "EWResidence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "EWGamepadModule.h"
#include "GameFramework/PlayerInput.h"

AEWCharacter::AEWCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(38, 88);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(GetCapsuleComponent()); Camera->SetRelativeLocation(FVector(0, 0, 74));
    Camera->bUsePawnControlRotation = true; Camera->FieldOfView = 85;
    bUseControllerRotationYaw = true;
    auto* Move = GetCharacterMovement();
    Move->MaxWalkSpeed = 350; Move->MaxStepHeight = 40; Move->SetWalkableFloorAngle(45);
    // A deliberate jump clears the 1.24m gallery rail; steer back onto a lower
    // gallery while falling. Ordinary walking still meets the visible guard.
    Move->JumpZVelocity = 570; Move->AirControl = .55; Move->BrakingDecelerationWalking = 1800;
    JumpMaxCount = 2;
    Move->bOrientRotationToMovement = false;
}

void AEWCharacter::BeginPlay()
{
    Super::BeginPlay(); bStreamingHold = false; SetStreamingHold(true); ResetSafeLocation();
}

bool AEWCharacter::AcceptInput() const
{
    auto* GI = GetGameInstance<UEWGameInstance>();
    return GI && !GI->bScriptedWorldAudit && GI->SessionStarted() && GI->Menu() == EEWMenu::None && !bStreamingHold;
}
void AEWCharacter::Forward(float V)
{
    if (AcceptInput()) { if (FMath::Abs(V)>.01) { if (bSeated && !LeaveSeat()) return; ++ForwardEvents; } AddMovementInput(FRotator(0, GetControlRotation().Yaw, 0).Vector(), V); }
}
void AEWCharacter::Right(float V)
{
    if (AcceptInput()) { if (FMath::Abs(V)>.01) { if (bSeated && !LeaveSeat()) return; ++StrafeEvents; } AddMovementInput(FRotationMatrix(FRotator(0, GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), V); }
}
// Preserve the mouse response that previously included the legacy controller
// scales (yaw +2.5, pitch -2.5), while sticks use explicit degrees per second.
void AEWCharacter::Turn(float V) { if (AcceptInput()) { if (FMath::Abs(V)>.01) ++LookEvents; AddControllerYawInput(V * 1.375f); } }
void AEWCharacter::Look(float V) { if (AcceptInput()) { if (FMath::Abs(V)>.01) ++LookEvents; AddControllerPitchInput(V * -1.375f); } }
void AEWCharacter::TurnPad(float V)
{
    if (AcceptInput() && FMath::Abs(V) > .001f)
    {
        auto* GI = GetGameInstance<UEWGameInstance>(); ++LookEvents;
        AddControllerYawInput(V * GI->PadLookSpeed * GetWorld()->GetDeltaSeconds());
    }
}
void AEWCharacter::LookPad(float V)
{
    if (AcceptInput() && FMath::Abs(V) > .001f)
    {
        auto* GI = GetGameInstance<UEWGameInstance>(); ++LookEvents;
        // SceneViewport reverses Gamepad_RightY before forwarding the axis to
        // PlayerInput. Undo that conversion: stick up must produce pitch up.
        AddControllerPitchInput(V * GI->PadLookSpeed * .75f * GetWorld()->GetDeltaSeconds() * (GI->bInvertPadY ? 1.f : -1.f));
    }
}
void AEWCharacter::StartJump() { if (AcceptInput() && !bLiftRiding) { if (bSeated) { LeaveSeat(); return; } ++JumpEvents; Jump(); } }
void AEWCharacter::OpenTerminal(){if(auto* G=GetGameInstance<UEWGameInstance>())if(G->Terminal)G->Terminal->Open();}
void AEWCharacter::OpenJournal() { if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->ToggleJournal(); }
void AEWCharacter::OpenMenu() { if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->ToggleMenu(); }
void AEWCharacter::RecordPlace() { if (AcceptInput()) if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->Interact(); }
void AEWCharacter::SprintOn() { if (AcceptInput()) { bSprint = true; GetCharacterMovement()->MaxWalkSpeed = 650; } }
void AEWCharacter::SprintOff() { bSprint = false; GetCharacterMovement()->MaxWalkSpeed = 350; }
void AEWCharacter::ClearHeldInput() { VoiceOff(); SprintOff(); StopJumping(); GetCharacterMovement()->StopMovementImmediately(); }
void AEWCharacter::Screenshot() { if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->Capture(); }
void AEWCharacter::Photo(){if(auto* G=GetGameInstance<UEWGameInstance>())if(G->PhotoMode)G->PhotoMode->Open();}
void AEWCharacter::Fullscreen()
{
    if (GEngine && GEngine->GetGameUserSettings())
    {
        if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->Graphics.BeginDisplayChange();
        auto* S = GEngine->GetGameUserSettings();
        auto CurrentMode = S->GetFullscreenMode();
        if (GEngine->GameViewport)
            if (auto Window = GEngine->GameViewport->GetWindow()) CurrentMode = Window->GetWindowMode();
        S->SetFullscreenMode(CurrentMode == EWindowMode::Windowed ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
        S->ApplyResolutionSettings(false);
        if (S->GetFullscreenMode() == EWindowMode::Windowed)
        {
            // A fullscreen startup can retain a hidden pre-fullscreen window
            // placement. Restore after the queued resize has applied it.
            FTimerHandle RestoreHandle;
            GetWorldTimerManager().SetTimer(RestoreHandle, FTimerDelegate::CreateWeakLambda(this, []
            {
                if (GEngine && GEngine->GameViewport)
                    if (auto Window = GEngine->GameViewport->GetWindow())
                        if (Window->GetWindowMode() == EWindowMode::Windowed) Window->Restore();
            }), .15f, false);
        }
        if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->Graphics.Apply();
    }
}
void AEWCharacter::Diagnostics()
{
    if (auto* GI = GetGameInstance<UEWGameInstance>()) { GI->bDiagnostics = !GI->bDiagnostics; GI->RefreshUI(); }
}
void AEWCharacter::VoiceOn(){if(AcceptInput())if(auto* G=GetGameInstance<UEWGameInstance>())if(G->SocialSession)G->SocialSession->PushToTalk(true);}
void AEWCharacter::VoiceOff(){if(auto* G=GetGameInstance<UEWGameInstance>())if(G->SocialSession)G->SocialSession->PushToTalk(false);}
void AEWCharacter::CityMenu(){if(auto* G=GetGameInstance<UEWGameInstance>())G->SetMenu(G->Menu()==EEWMenu::City?EEWMenu::None:EEWMenu::City);}
void AEWCharacter::SetupPlayerInputComponent(UInputComponent* I)
{
    Super::SetupPlayerInputComponent(I);
    I->BindAxis(TEXT("MoveForward"), this, &AEWCharacter::Forward);
    I->BindAxis(TEXT("MoveRight"), this, &AEWCharacter::Right);
    I->BindAxis(TEXT("Turn"), this, &AEWCharacter::Turn);
    I->BindAxis(TEXT("LookUp"), this, &AEWCharacter::Look);
    I->BindAxis(TEXT("TurnPad"), this, &AEWCharacter::TurnPad);
    I->BindAxis(TEXT("LookPad"), this, &AEWCharacter::LookPad);
    I->BindAction(TEXT("Jump"), IE_Pressed, this, &AEWCharacter::StartJump);
    I->BindAction(TEXT("Jump"), IE_Released, this, &AEWCharacter::StopJumping);
    I->BindAction(TEXT("Journal"), IE_Pressed, this, &AEWCharacter::OpenJournal);
    I->BindAction(TEXT("Menu"), IE_Pressed, this, &AEWCharacter::OpenMenu);
    I->BindAction(TEXT("Record"), IE_Pressed, this, &AEWCharacter::RecordPlace);
    I->BindAction(TEXT("Sprint"), IE_Pressed, this, &AEWCharacter::SprintOn);
    I->BindAction(TEXT("Sprint"), IE_Released, this, &AEWCharacter::SprintOff);
    I->BindAction(TEXT("Screenshot"), IE_Pressed, this, &AEWCharacter::Screenshot);
    I->BindAction(TEXT("Fullscreen"), IE_Pressed, this, &AEWCharacter::Fullscreen);
    I->BindKey(EKeys::F2, IE_Pressed, this, &AEWCharacter::Diagnostics);
    I->BindKey(EKeys::V, IE_Pressed, this, &AEWCharacter::VoiceOn);
    I->BindKey(EKeys::V, IE_Released, this, &AEWCharacter::VoiceOff);
    I->BindKey(EKeys::T, IE_Pressed, this, &AEWCharacter::CityMenu);
    I->BindKey(EKeys::F9, IE_Pressed, this, &AEWCharacter::Photo);
    I->BindKey(EKeys::Q,IE_Pressed,this,&AEWCharacter::OpenTerminal);
    I->BindKey(EKeys::Gamepad_RightThumbstick,IE_Pressed,this,&AEWCharacter::OpenTerminal);
}

void AEWCharacter::SetStreamingHold(bool Hold)
{
    if (bStreamingHold == Hold) return;
    bStreamingHold = Hold;
    if (Hold) { if (bSeated) LeaveSeat(); GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->DisableMovement(); }
    else if (!bLiftRiding && !bSeated) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}
void AEWCharacter::SetLiftRiding(bool Riding)
{
    if (Riding && bSeated) LeaveSeat();
    bLiftRiding=Riding;ClearHeldInput();
    if(Riding)GetCharacterMovement()->DisableMovement();
    else if(!bStreamingHold)GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}
void AEWCharacter::ResetSafeLocation() { LastSafe = GetActorLocation();LastSafeMovingBase.Reset(); }
bool AEWCharacter::SitAt(AEWResidence* Residence)
{
    return IsValid(Residence) && SitAtTransform(Residence,Residence->SeatTransform(),Residence->StandTransform());
}
bool AEWCharacter::SitAtTransform(AActor* SeatingActor,const FTransform& Seat,const FTransform& Stand)
{
    if (!IsValid(SeatingActor) || bSeated || bStreamingHold || bLiftRiding) return false;
    ClearHeldInput();
    SeatStandTransform = Stand;
    GetCapsuleComponent()->SetCapsuleHalfHeight(42, false);
    SetActorLocationAndRotation(Seat.GetLocation(), FRotator(0,Seat.Rotator().Yaw,0), false, nullptr, ETeleportType::TeleportPhysics);
    if (Controller) Controller->SetControlRotation(Seat.Rotator());
    SeatOwner = SeatingActor; bSeated = true;
    Camera->SetRelativeLocation(FVector(0, 0, 47));
    GetCharacterMovement()->DisableMovement();
    return true;
}
void AEWCharacter::UpdateTransitSeat(AActor* SeatingActor,const FVector& Position,const FTransform& Stand,bool Locked)
{
    if(!bSeated || SeatOwner.Get()!=SeatingActor)return;
    bTransitSeatLocked=Locked;SeatStandTransform=Stand;
    SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
}
bool AEWCharacter::LeaveSeat()
{
    if (!bSeated) return true;
    if(bTransitSeatLocked && SeatOwner.IsValid())return false;
    FVector Position = SeatStandTransform.GetLocation();
    FRotator Rotation = SeatStandTransform.Rotator();
    // Find the return position with the full standing capsule, then commit.
    GetCapsuleComponent()->SetCapsuleHalfHeight(88, false);
    if (!GetWorld()->FindTeleportSpot(this, Position, Rotation))
    {
        Position = LastSafe;
        if (!GetWorld()->FindTeleportSpot(this, Position, Rotation))
        { GetCapsuleComponent()->SetCapsuleHalfHeight(42, false); if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->Notify(TEXT("立つ場所を確認できませんでした。もう一度お試しください。")); return false; }
    }
    bSeated = false; bTransitSeatLocked=false; SeatOwner.Reset();
    Camera->SetRelativeLocation(FVector(0, 0, 74));
    SetActorLocationAndRotation(Position, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    if (Controller) Controller->SetControlRotation(Rotation);
    LastSafe = Position;
    ClearHeldInput();
    if (!bStreamingHold && !bLiftRiding) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    return true;
}
FTransform AEWCharacter::BookmarkTransform() const
{ return bSeated ? SeatStandTransform : GetActorTransform(); }
void AEWCharacter::ShiftLocalOrigin(const FVector& Offset)
{
    // A plain SetActorLocation leaves CharacterMovement's OldBaseLocation in
    // the old coordinate frame, so the moved floor can carry us a second time.
    // false updates the real physics/render transforms: this is a local shift,
    // not an engine-wide physics-scene origin shift.
    ApplyWorldOffset(Offset,false);
    LastSafe+=Offset;
    if (bSeated) SeatStandTransform.AddToTranslation(Offset);
    GetCharacterMovement()->bForceNextFloorCheck=true;
}
void AEWCharacter::SetTestMovement(const FVector& Direction, bool Enabled)
{
    TestDirection = Direction.GetSafeNormal2D(); bTestMovement = Enabled;
}
void AEWCharacter::Tick(float Delta)
{
    Super::Tick(Delta);
    if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->UpdatePresentationState();
    if (bSeated && !SeatOwner.IsValid()) LeaveSeat();
    const auto& Support=GetCharacterMovement()->CurrentFloor.HitResult.Component;
    if (!bStreamingHold && !bLiftRiding && !bSeated && GetCharacterMovement()->IsMovingOnGround() &&
        Support.IsValid() && Support->ComponentHasTag(TEXT("EWFloor")))
    {
        LastSafe=GetActorLocation();
        LastSafeMovingBase=Cast<AEWAeroYacht>(Support->GetOwner());
        if(LastSafeMovingBase.IsValid())LastSafeOnBase=LastSafeMovingBase->GetActorTransform().InverseTransformPosition(LastSafe);
    }
    const auto* Instance=GetGameInstance<UEWGameInstance>();
    const auto* Manager=Instance?Instance->Manager.Get():nullptr;
    if (!bStreamingHold && !bLiftRiding && !bSeated && Manager &&
        GetActorLocation().Z < Manager->FallRecoveryHeight())
    {
        const FVector Safe=LastSafeMovingBase.IsValid()?LastSafeMovingBase->GetActorTransform().TransformPosition(LastSafeOnBase):LastSafe;
        SetActorLocation(Safe + FVector(0, 0, 10), false, nullptr, ETeleportType::TeleportPhysics);
        GetCharacterMovement()->StopMovementImmediately(); ++FallRecoveries;
        if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->Notify(TEXT("最後の足場へ戻りました。"));
    }
    if (bTestMovement && !bStreamingHold && !bSeated) AddMovementInput(TestDirection, 1);
}

void AEWPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalController() && GEngine && GEngine->GameViewport && !FParse::Param(FCommandLine::Get(),TEXT("EWCinematic95")))
    {
        Overlay = SNew(SEWOverlay).Owner(GetGameInstance<UEWGameInstance>());
        GEngine->GameViewport->AddViewportWidgetContent(Overlay.ToSharedRef(), 10);
        RefreshInterface();
    }
}
void AEWPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Overlay.IsValid() && GEngine && GEngine->GameViewport)
        GEngine->GameViewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
    Overlay.Reset(); Super::EndPlay(Reason);
}
bool AEWPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
    if (const auto* GI = GetGameInstance<UEWGameInstance>())
        if (GI->bScriptedWorldAudit) return true;
    if (Params.Event != IE_Released && FMath::Abs(Params.AmountDepressed) > .05)
        if (auto* GI = GetGameInstance<UEWGameInstance>())
        {
            const auto* Pad = FEWGamepadModule::GetIfLoaded();
            GI->NoteInput(Params.Key.IsGamepadKey(), Pad && Pad->RecentSonyInput());
        }
    return Super::InputKey(Params);
}
FString AEWPlayerController::FocusedControlLabel() const{return Overlay?Overlay->FocusedLabel():FString();}
void AEWPlayerController::RefreshInterface()
{
    auto* GI = GetGameInstance<UEWGameInstance>();
    if (!GI || !Overlay) return;
    Overlay->Rebuild();
    const bool Open = GI->Menu() != EEWMenu::None;
    bShowMouseCursor = Open && !GI->bUsingGamepad; bEnableClickEvents = Open;
    if (Open)
    {
        FInputModeGameAndUI Mode;
        const bool TerminalOpen=GI->Menu()==EEWMenu::Terminal && GI->Terminal;
        const bool TabletOpen=GI->Menu()==EEWMenu::Monitor && GI->ActiveMediaScreen;
        Mode.SetWidgetToFocus(TabletOpen?GI->ActiveMediaScreen->TabletFocusWidget():TerminalOpen?GI->Terminal->FocusWidget():TSharedPtr<SWidget>(Overlay)); Mode.SetHideCursorDuringCapture(false);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
        if (auto* P = Cast<AEWCharacter>(GetPawn())) P->ClearHeldInput();
        if (PlayerInput) PlayerInput->FlushPressedKeys();
        if(TabletOpen)GI->ActiveMediaScreen->FocusTablet();else if(TerminalOpen)GI->Terminal->FocusFirst();else Overlay->FocusInitialControl();
    }
    else
    {
        SetInputMode(FInputModeGameOnly());
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
}

AEWGameMode::AEWGameMode()
{
    DefaultPawnClass = AEWCharacter::StaticClass(); PlayerControllerClass = AEWPlayerController::StaticClass();
}
void AEWGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
    { if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->AttachWorld(); }));
}
