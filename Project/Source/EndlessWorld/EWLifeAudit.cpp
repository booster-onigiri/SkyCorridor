#include "EWLifeAudit.h"
#include "EWGameInstance.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWFishing.h"
#include "EWPhotoMode.h"
#include "EWSocialSession.h"
#include "EWDayCycle.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

AEWLifeAudit::AEWLifeAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWLifeAudit::BeginPlay()
{
    Super::BeginPlay();Started=FPlatformTime::Seconds();Next=Started+10;
    FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);
    FParse::Value(FCommandLine::Get(),TEXT("EWLifeExpectedCatches="),ExpectedBefore);
    FString Data;
    if(!FParse::Value(FCommandLine::Get(),TEXT("EWDataDir="),Data) || Report.IsEmpty() || FPaths::FileExists(Report))
    {Finished=true;FPlatformMisc::RequestExit(false);}
}
void AEWLifeAudit::Check(bool OK,const FString& Name)
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),Name);O->SetBoolField(TEXT("passed"),OK);
    Checks.Add(MakeShared<FJsonValueObject>(O));Passed&=OK;
}
void AEWLifeAudit::Finish(const FString& Failure)
{
    if(Finished)return;Finished=true;
    auto* G=GetGameInstance<UEWGameInstance>();
    if(!Failure.IsEmpty())Check(false,Failure);
    if(G)Check(G->SaveNow(),TEXT("explicit_save_succeeds"));
    if(G && G->DayCycle && ClockStarted>0)
    {
        const auto Day=G->DayCycle->Evidence();
        const double Actual=FMath::Fmod(Day->GetNumberField(TEXT("game_hour"))-InitialHour+24.,24.);
        Check(Day->GetNumberField(TEXT("day_seconds"))==1800,TEXT("runtime_day_lasts_30_minutes"));
        Check(FMath::IsNearlyEqual(Actual,(FPlatformTime::Seconds()-ClockStarted)/75.,.005),TEXT("runtime_clock_advances_without_time_override"));
    }
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("passed"),Passed);O->SetArrayField(TEXT("checks"),Checks);
    O->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Started);O->SetNumberField(TEXT("catches_before"),Before);
    TSharedPtr<FJsonObject> State;
    if(G && G->Fishing && EWSocial::Decode(G->Fishing->FishingEvidence(),State))O->SetObjectField(TEXT("fishing"),State);
    if(G && G->PhotoMode && EWSocial::Decode(G->PhotoMode->PhotoEvidence(),State))O->SetObjectField(TEXT("photo"),State);
    if(G && G->DayCycle)O->SetObjectField(TEXT("day_cycle"),G->DayCycle->Evidence());
    if(ClockStarted>0){O->SetNumberField(TEXT("initial_hour"),InitialHour);O->SetNumberField(TEXT("clock_elapsed_seconds"),FPlatformTime::Seconds()-ClockStarted);}
    O->SetStringField(TEXT("scope"),TEXT("Single-PC real gameplay and PNG export. No EOS, physical audio or human input acceptance."));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);
    FFileHelper::SaveStringToFile(EWSocial::Encode(O),*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExit(false);
}
void AEWLifeAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Finished)return;const double Now=FPlatformTime::Seconds();
    if(Now-Started>180){Finish(TEXT("gameplay_timeout"));return;}if(Now<Next)return;
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(!G || !G->Manager || !G->Fishing || !G->PhotoMode || !P || !G->SessionStarted())return;
    auto* F=G->Fishing.Get();auto* Photo=G->PhotoMode.Get();
    auto Count=[&](){int32 N=0;for(const auto& R:F->Records())N+=int32(R.Count);return N;};
    if(Phase==0)
    {
        if(G->Manager->IsTravelling() || !P->GetCharacterMovement()->IsMovingOnGround())return;
        if(G->DayCycle && !G->DayCycle->Evidence()->GetBoolField(TEXT("audit_override")))
        {InitialHour=G->DayCycle->Evidence()->GetNumberField(TEXT("game_hour"));ClockStarted=Now;}
        Before=Count();if(ExpectedBefore>=0)Check(Before==ExpectedBefore,TEXT("restart_preserves_catch_count"));
        Check(G->SocialSession && G->SocialSession->LocalSaveStore().Integrity(),TEXT("social_database_open"));
        TSharedPtr<FJsonObject> E;EWSocial::Decode(F->FishingEvidence(),E);Check(E && E->GetBoolField(TEXT("surface_loaded")),TEXT("cooked_fishing_material_loaded"));
        F->VisitSpot(0);Phase=1;Next=Now+3;return;
    }
    if(Phase==1)
    {
        if(G->Manager->IsTravelling() || !P->GetCharacterMovement()->IsMovingOnGround())return;
        const auto& S=EWFishing::Spots()[Site];FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(LifeAuditWater),false,P);Q.AddIgnoredActor(F);
        Check(!GetWorld()->LineTraceSingleByChannel(Hit,P->Camera->GetComponentLocation(),G->Manager->ToRender(S.Coord,S.Float),ECC_Visibility,Q),FString::Printf(TEXT("site%d_real_water_visible"),Site));
        Check(F->Interact() && F->Phase()==EWFishing::EPhase::Waiting,FString::Printf(TEXT("site%d_real_cast_starts"),Site));
        if(!Passed){Finish();return;}Phase=2;return;
    }
    if(Phase==2)
    {
        if(F->Phase()!=EWFishing::EPhase::Bite)return;
        Check(F->Interact() && F->Phase()==EWFishing::EPhase::Reeling,FString::Printf(TEXT("site%d_single_press_reels"),Site));Phase=3;return;
    }
    if(Phase==3)
    {
        if(F->Phase()!=EWFishing::EPhase::Landed)return;
        Check(F->Saved() && Count()==Before+Site+1 && F->LastCatch().Valid(),FString::Printf(TEXT("site%d_catch_saved_once"),Site));
        F->DisplayFish(F->LastCatch().Species);Check(F->DisplayedFish()==F->LastCatch().Species,FString::Printf(TEXT("site%d_display_selection_saved"),Site));
        if(++Site<3){F->VisitSpot(Site);Phase=1;Next=Now+3;return;}
        F->VisitSpot(0);Phase=4;Next=Now+3;return;
    }
    if(Phase==4)
    {
        if(G->Manager->IsTravelling() || !P->GetCharacterMovement()->IsMovingOnGround())return;
        Photo->Open();Check(Photo->Active(),TEXT("photo_camera_opens"));
        // Clock-quay display: turn from the water towards the exhibit without moving the player.
        const FVector Target=G->Manager->ToRender({0,0},EWFishing::DisplayPosition()+FVector(0,0,126));
        const FRotator Aim=(Target-P->Camera->GetComponentLocation()).Rotation();
        double Turn=FRotator::NormalizeAxis(Aim.Yaw-P->Camera->GetComponentRotation().Yaw);
        for(int32 I=0;I<5 && FMath::Abs(Turn)>.01;++I){const double Step=FMath::Clamp(Turn,-45.,45.);Photo->Turn(Step);Turn-=Step;}
        Photo->Tilt(Aim.Pitch-P->Camera->GetComponentRotation().Pitch);
        Photo->Shoot();Phase=5;Next=Now+1;return;
    }
    if(Phase==5)
    {
        if(Photo->Busy())return;
        Check(!Photo->LastFile().IsEmpty() && IFileManager::Get().FileSize(*Photo->LastFile())>0,TEXT("landscape_png_saved"));
        PreviousPhoto=Photo->LastFile();Photo->TogglePortrait();Photo->ToggleSelfie();Photo->Shoot();Phase=6;Next=Now+1;return;
    }
    if(Phase==6)
    {
        if(Photo->Busy())return;
        Check(Photo->LastFile()!=PreviousPhoto && IFileManager::Get().FileSize(*Photo->LastFile())>0,TEXT("portrait_selfie_png_saved"));
        TSharedPtr<FJsonObject> View;EWSocial::Decode(Photo->PhotoEvidence(),View);
        Check(View && View->GetBoolField(TEXT("subject_unobstructed")),TEXT("selfie_is_not_hidden_by_aquarium"));
        PreviousPhoto=Photo->LastFile();Photo->CycleTimer();Photo->Shoot();G->SetMenu(EEWMenu::None);
        Check(!Photo->Busy() && !Photo->Active(),TEXT("leaving_cancels_ten_second_timer"));Phase=7;Next=Now+11;return;
    }
    if(Phase==7)
    {
        Check(Photo->LastFile()==PreviousPhoto && !Photo->Busy(),TEXT("cancelled_timer_does_not_save_late"));
        auto* PC=UGameplayStatics::GetPlayerController(this,0);Check(PC && PC->GetViewTarget()==P,TEXT("player_camera_restored"));
        Finish();
    }
}
