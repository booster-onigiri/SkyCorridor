#include "EWHotelAudit.h"
#include "EWHotelPlan.h"
#include "EWInteriors.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWLift.h"
#include "EWDayCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"

AEWHotelAudit::AEWHotelAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWHotelAudit::BeginPlay()
{
    Super::BeginPlay();Started=Stage=Progress=FPlatformTime::Seconds();Resume=FParse::Param(FCommandLine::Get(),TEXT("EWHotel83Resume"));
    if(Resume)Phase=-1;
    if(!FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report) || IFileManager::Get().FileExists(*Report)){Done=true;FPlatformMisc::RequestExit(false);}
}
bool AEWHotelAudit::Check(bool Pass,const FString& Name)
{
    auto C=MakeShared<FJsonObject>();C->SetBoolField(TEXT("pass"),Pass);C->SetStringField(TEXT("check"),Name);Checks.Add(MakeShared<FJsonValueObject>(C));
    if(!Pass)Finish(Name);return Pass;
}
void AEWHotelAudit::Finish(const FString& Error)
{
    if(Done)return;Done=true;auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(P)P->SetTestMovement(FVector::ZeroVector,false);
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);O->SetArrayField(TEXT("checks"),Checks);
    O->SetBoolField(TEXT("parallel_rhi_translate"),IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmd.ParallelTranslate.Enable"))->GetBool());
    O->SetNumberField(TEXT("submission_thread"),IConsoleManager::Get().FindConsoleVariable(TEXT("rhi.UseSubmissionThread"))->GetInt());
    O->SetArrayField(TEXT("captures"),Captures);O->SetNumberField(TEXT("walked_metres"),Walked/100.);O->SetNumberField(TEXT("fall_recoveries"),P?P->FallRecoveries-Falls:0);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    O->SetStringField(TEXT("scope"),TEXT("isolated local character movement, 8 suites, 4 new lift stops, silent four-time-of-day captures; online occupancy not exercised"));
    FString T;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&T));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);FFileHelper::SaveStringToFile(T,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);FPlatformMisc::RequestExit(false);
}
void AEWHotelAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>720 || Age>100){Finish(FString::Printf(TEXT("timeout phase %d room %d point %d ride %d"),Phase,Index,Point,Ride));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !M || !P || !PC || M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt({0,0}))return;
    auto* Move=P->GetCharacterMovement();
    auto Next=[&](int32 N){Phase=N;Stage=Progress=Now;Best=DBL_MAX;Last=P->GetActorLocation();
        auto Status=MakeShared<FJsonObject>();Status->SetNumberField(TEXT("phase"),Phase);Status->SetNumberField(TEXT("suite"),101+Index);Status->SetNumberField(TEXT("ride"),Ride);Status->SetNumberField(TEXT("seconds"),Now-Started);
        Status->SetBoolField(TEXT("parallel_rhi_translate"),IConsoleManager::Get().FindConsoleVariable(TEXT("r.RHICmd.ParallelTranslate.Enable"))->GetBool());
        Status->SetNumberField(TEXT("submission_thread"),IConsoleManager::Get().FindConsoleVariable(TEXT("rhi.UseSubmissionThread"))->GetInt());
        FString Text;FJsonSerializer::Serialize(Status,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Report+TEXT(".progress.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        UE_LOG(LogTemp,Display,TEXT("HOTEL83_STAGE %d room %d ride %d"),N,Index,Ride);};
    auto Walk=[&](FVector Goal)->bool
    {
        const FVector Pos=P->GetActorLocation();Walked+=FVector::Dist2D(Pos,Last);Last=Pos;const FVector D=Goal-Pos;const double Distance=D.Size2D();
        if(Distance<22){P->SetTestMovement(FVector::ZeroVector,false);return true;}
        if(Distance<Best-5){Best=Distance;Progress=Now;}else if(Now-Progress>7){Finish(FString::Printf(TEXT("walk blocked phase %d room %d point %d at %s goal %s"),Phase,Index,Point,*Pos.ToString(),*Goal.ToString()));return false;}
        PC->SetControlRotation(D.Rotation());P->SetTestMovement(D.GetSafeNormal2D(),true);return false;
    };
    // Resume follows the same action as the menu. The launcher leaves EWPlay
    // unset so the preview world cannot overwrite the stored upper-floor place.
    if(Phase==-1 && Move->IsMovingOnGround())
    {G->ContinueWorld();Next(0);return;}
    if(Phase==0 && Move->IsMovingOnGround())
    {
        Falls=P->FallRecoveries;const auto R=M->RecipeAt({0,0});if(!R)return;FString Error;
        if(!Check(!R->bFallback && R->Validate(Error),TEXT("reference city recipe remains valid")))return;
        for(int I=0;I<8;++I){const auto* Room=EWHotelPlan::Find(*R,I);if(!Check(Room!=nullptr,FString::Printf(TEXT("suite %d generated"),101+I)))return;Rooms.Add(*Room);}
        int32 Stops=0;for(const auto& L:R->Lifts)for(const auto& S:L.Stops)if(S.Label.StartsWith(TEXT("雲上レジデンス")))++Stops;
        if(!Check(Stops==4,TEXT("four added floors have real lift stops")))return;
        if(Resume)
        {
            const auto Q=Rooms[7].Frame.InverseTransformPosition(M->ToLocal({0,0},P->GetActorLocation()));
            if(!Check(FMath::Abs(Q.X)<60 && FMath::Abs(Q.Y+740)<60 && Move->IsMovingOnGround(),TEXT("extracted launcher restores the saved upper-suite entrance")))return;
            if(!Check(G->SaveNow(),TEXT("extracted release can save again")))return;Finish();return;
        }
        G->VisitSkyResidence(0);Next(1);return;
    }
    if(Phase==1 && Age>2 && Move->IsMovingOnGround())
    {
        Route=EWInteriors::Route(Rooms[Index]);Point=1;Move->MaxWalkSpeed=350;PC->SetViewTarget(P);G->SetMenu(EEWMenu::None);Next(2);return;
    }
    if(Phase==2)
    {
        if(Walk(M->ToRender({0,0},Route[Point])+FVector(0,0,90)))
        {
            const double ExpectedZ=M->ToRender({0,0},Route[Point]).Z+90;
            if(!Check(Move->IsMovingOnGround() && FMath::Abs(P->GetActorLocation().Z-ExpectedZ)<12 && P->FallRecoveries==Falls,FString::Printf(TEXT("suite %d route %d on physical floor"),101+Index,Point)))return;
            ++Point;Best=DBL_MAX;Progress=Now;
            if(Point==Route.Num()){Photo=0;Next(3);}return;
        }
        return;
    }
    if(Phase==3)
    {
        if(!Camera)Camera=GetWorld()->SpawnActor<ACameraActor>();
        const auto T=Rooms[Index].Frame;const bool Bed=Photo==4;
        const FVector Eye=M->ToRender({0,0},T.TransformPosition(Bed?FVector(110,-240,165):FVector(-110,-425,165)));
        const FVector Aim=M->ToRender({0,0},T.TransformPosition(Bed?FVector(460,235,138):FVector(-350,180,145)));
        Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(80);PC->SetViewTarget(Camera);
        const double Hours[]={7,12,18,23,12};G->DayCycle->SetAuditHour(Hours[Photo]);Next(4);return;
    }
    if(Phase==4 && Age>3)
    {
        auto Shot=MakeShared<FJsonObject>();Shot->SetNumberField(TEXT("suite"),101+Index);Shot->SetNumberField(TEXT("view"),Photo);Shot->SetObjectField(TEXT("lighting"),G->DayCycle->Evidence());Captures.Add(MakeShared<FJsonValueObject>(Shot));
        FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/FString::Printf(TEXT("suite-%d-view-%d.png"),101+Index,Photo),false,false);Next(5);return;
    }
    if(Phase==5 && Age>1)
    {
        if(++Photo<5){Next(3);return;}
        if(++Index<8){PC->SetViewTarget(P);G->VisitSkyResidence(Index);Next(1);return;}
        PC->SetViewTarget(P);G->DayCycle->SetAuditHour(12);Next(20);return;
    }
    if(Phase==20)
    {
        Lift=nullptr;
        const FString FirstRoom=FString::Printf(TEXT("%d–"),101+(Ride/2)*4);
        for(TActorIterator<AEWLift> It(GetWorld());It;++It)for(const auto& Stop:It->Spec.Stops)
            if(Stop.Label.StartsWith(TEXT("雲上レジデンス")) && Stop.Label.Contains(FirstRoom)){Lift=*It;break;}
        if(!Check(Lift!=nullptr,FString::Printf(TEXT("lift actor exists for tower %d"),Ride/2)))return;
        From=Lift->Spec.Stops.Num()-3+Ride%2;To=From+1;
        P->SetActorLocation(Lift->GetActorTransform().TransformPosition(Lift->Spec.Stops[From].Entry)+FVector(0,0,92),false);Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Walking);P->ResetSafeLocation();
        if(!Check(Lift->Call(From),TEXT("call existing car to departure")))return;Next(21);return;
    }
    if(Phase==21 && !Lift->IsMoving() && Age>2){Next(22);return;}
    if(Phase==22)
    {
        if(Walk(Lift->CarPosition()+FVector(0,0,90)))
        {if(!Check(Lift->Ride(P,To),FString::Printf(TEXT("physically board car for added stop %d"),Ride)))return;Next(23);}return;
    }
    if(Phase==23 && !Lift->IsMoving() && Age>1){Next(24);return;}
    if(Phase==24)
    {
        if(Walk(Lift->GetActorTransform().TransformPosition(Lift->Spec.Stops[To].Entry)+FVector(0,0,90)))
        {if(!Check(Move->IsMovingOnGround() && P->FallRecoveries==Falls && Lift->FloorIndex()==To,FString::Printf(TEXT("ride and walk out at added stop %d"),Ride)))return;
            FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/FString::Printf(TEXT("lift-stop-%d.png"),Ride),false,false);Next(25);}return;
    }
    if(Phase==25 && Age>1)
    {if(++Ride<4){Next(20);return;}G->VisitSkyResidence(7);PC->SetViewTarget(P);Next(30);return;}
    if(Phase==30 && Age>3 && Move->IsMovingOnGround())
    {
        if(!Check(P->FallRecoveries==Falls,TEXT("no fallback recovery across rooms and lifts")))return;
        G->SetMenu(EEWMenu::SkyResidences);Next(31);return;
    }
    if(Phase==31 && Age>2){FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/TEXT("suite-menu.png"),true,false);Next(32);return;}
    if(Phase==32 && Age>1){G->SetMenu(EEWMenu::None);if(!Check(G->SaveNow(),TEXT("normal save persists upper-floor visit")))return;Finish();}
}
