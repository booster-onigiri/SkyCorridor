#include "EWSkyrailAudit.h"
#include "EWSkyrail.h"
#include "EWSkyrailPlan.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWLift.h"
#include "EWSaveStore.h"
#include "EWOuterWater.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "RHI.h"

AEWSkyrailAudit::AEWSkyrailAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWSkyrailAudit::BeginPlay()
{Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();Report=FPaths::ProjectSavedDir()/TEXT("Verification/skyrail.json");FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);Resume=FParse::Param(FCommandLine::Get(),TEXT("EWGlass88Resume"));if(Resume)Phase=-1;}
void AEWSkyrailAudit::Capture(const FString& N){if(!GUsingNullRHI)FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/N+TEXT(".png"),false,false);}
bool AEWSkyrailAudit::Check(bool OK,const FString& N)
{auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("pass"),OK);O->SetStringField(TEXT("check"),N);Checks.Add(MakeShared<FJsonValueObject>(O));if(!OK)Finish(N);return OK;}
void AEWSkyrailAudit::Finish(const FString& Error)
{
    if(Finished)return;Finished=true;auto* G=GetGameInstance<UEWGameInstance>();
    if(auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))P->SetTestMovement(FVector::ZeroVector,false);
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);
    O->SetArrayField(TEXT("checks"),Checks);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    if(G && G->Skyrail)O->SetObjectField(TEXT("rail"),G->Skyrail->Evidence());
    O->SetBoolField(TEXT("resume"),Resume);O->SetNumberField(TEXT("origin_rebases"),G && G->Manager?G->Manager->RebaseCount:0);
    if(auto* Player=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))O->SetNumberField(TEXT("fall_recoveries"),Player->FallRecoveries);
    O->SetStringField(TEXT("method"),TEXT("Real CharacterMovement and production E interaction: two-station direct service, water-city lift and sanctuary walk, 656m return trip, rebase and in-flight save; private fixture; silent"));
    FString T;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&T));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);
    FFileHelper::SaveStringToFile(T,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);FPlatformMisc::RequestExit(false);
}
void AEWSkyrailAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Finished)return;const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>540){Finish(FString::Printf(TEXT("timeout phase %d"),Phase));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!G || !G->Manager || !G->Skyrail || !P || !PC)return;
    auto* T=G->Skyrail.Get();auto* M=G->Manager.Get();auto* Move=P->GetCharacterMovement();
    if(M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt(M->PlayerCoord()) || (Phase<=1 && M->ReadyCount()<49))return;
    auto Next=[&](int32 I){Phase=I;Stage=Now;FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"phase\":%d,\"seconds\":%.2f}"),Phase,Now-Started),*(Report+TEXT(".progress.json")));};
    auto Walk=[&](FVector Target)
    {
        const FVector D=Target-P->GetActorLocation();if(D.Size2D()<26){P->SetTestMovement(FVector::ZeroVector,false);return true;}
        P->SetTestMovement(D,true);PC->SetControlRotation(D.Rotation());return false;
    };
    if(Phase==-1 && Move->IsMovingOnGround()){G->ContinueWorld();Next(-2);return;}
    if(Phase==-2 && Age>3)
    {
        EW::PlaceBookmark Save;const auto At=M->CurrentPosition();
        if(!Check(Move->IsMovingOnGround() && M->PlayerCoord()==EW::ChunkCoord{0,0} && FVector::Dist2D(At.LocalPosition,EWSkyrailPlan::Boarding(0)+FVector(1120,0,0))<60,TEXT("extracted build restores the real return platform")))return;
        if(!Check(P->FallRecoveries==0 && G->SaveNow() && G->Store()->LoadCurrent(Save) && Save.LocalPosition.Equals(At.LocalPosition,1),TEXT("restored player can save again without recovery")))return;
        Capture(TEXT("restored-station"));Next(-3);return;
    }
    if(Phase==-3 && Age>2){Finish();return;}
    if(Phase==0){G->VisitSkyrail();Next(1);return;}
    if(!T->IsReady())return;
    if(Phase==1 && Age>4)
    {
        if(!Check(Move->IsMovingOnGround() && FVector::Dist(P->GetActorLocation(),T->BoardingPosition(0))<25,TEXT("station menu arrives on physical platform")))return;
        for(int32 I=0;I<2;++I)
        {
            const auto R=M->RecipeAt(EWSkyrailPlan::StopChunk(I));if(!Check(R && !R->bFallback && R->Lifts.ContainsByPredicate([](const EW::LiftSpec& L){return L.Id.StartsWith(TEXT("skyrail78/"));}),TEXT("station and lift in streamed recipe")))return;
            // Remote scenery deliberately has no collision until approached.
            // Verify the destination floor on arrival, after its chunk streams in.
            if(I==0)
            {
                FHitResult Hit;const FVector B=T->LiftEntry(I);
                if(!Check(GetWorld()->LineTraceSingleByChannel(Hit,B,B-FVector(0,0,200),ECC_Visibility) && Hit.Component.IsValid() && Hit.Component->ComponentHasTag(TEXT("EWFloor")),TEXT("departure lift approach is on real walking floor")))return;
            }
        }
        Camera=GetWorld()->SpawnActor<ACameraActor>();const FVector Target=M->ToRender({0,0},EWSkyrailPlan::Stop(0)+FVector(0,0,130));
        const FVector Eye=Target+FVector(-2200,-1050,370);Camera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation());
        Camera->GetCameraComponent()->SetFieldOfView(85);PC->SetViewTarget(Camera);Next(2);return;
    }
    if(Phase==2 && Age>6){Capture(TEXT("train-exterior"));Next(3);return;}
    if(Phase==3 && Age>1)
    {
        PC->SetViewTarget(P);PC->SetControlRotation(FRotator(0,270,0));
        if(T->AtStation()!=0 || T->Evidence()->GetNumberField(TEXT("door_open"))<.99)return;
        G->Interact();if(!Check(T->IsRider(P),TEXT("E boards only a stopped train with open doors")))return;Next(4);return;
    }
    if(Phase==4 && T->AtStation()==INDEX_NONE)
    {
        if(!Check(!P->LeaveSeat(),TEXT("movement cannot alight between stations")))return;
        G->Interact();if(!Check(T->IsRider(P),TEXT("E cannot alight during travel")))return;
        PC->SetControlRotation(FRotator(0,280,0));Next(5);return;
    }
    if(Phase==5)
    {

        if(!Crossed && M->PlayerCoord()==EW::ChunkCoord{1,0})
        {
            Crossed=true;EW::PlaceBookmark Save;const auto At=M->CurrentPosition();
            if(!Check(At.Coord==EW::ChunkCoord{0,0} && G->SaveNow() && G->Store()->LoadCurrent(Save) && Save.Coord==At.Coord && Save.LocalPosition.Equals(At.LocalPosition,1),TEXT("in-flight cross-chunk save keeps departure platform coordinates")))return;
            Capture(TEXT("train-carriage"));
        }
        if(T->AtStation()!=1 || T->Evidence()->GetNumberField(TEXT("door_open"))<.99)return;
        if(!Check(Crossed,TEXT("train carries passenger into next city chunk")))return;
        if(!Check(M->PlayerCoord()==EW::ChunkCoord{3,0},TEXT("through service reaches water city past three chunk boundaries")))return;
        FHitResult Hit;const FVector Entry=T->LiftEntry(1);
        if(!Check(M->ReadyAt({3,0}) && GetWorld()->LineTraceSingleByChannel(Hit,Entry,Entry-FVector(0,0,200),ECC_Visibility) && Hit.Component.IsValid() && Hit.Component->ComponentHasTag(TEXT("EWFloor")),TEXT("destination lift floor exists after streaming into water city")))return;
        if(!Check(T->Evidence()->GetNumberField(TEXT("peak_speed_kmh"))>38,TEXT("faster cruise above 38 km/h")))return;
        G->Interact();if(!Check(!P->IsSeated() && FVector::Dist(P->GetActorLocation(),T->BoardingPosition(1))<25,TEXT("E alights at the water-city platform")))return;Next(6);return;
    }
    if(Phase==6)
    {
        if(!Walk(T->BoardingPosition(1)+FVector(1930,70,0)))return;
        for(TActorIterator<AEWLift> It(GetWorld());It;++It)if(It->Spec.Id==TEXT("skyrail78/2"))Lift=*It;
        if(!Check(Lift.IsValid() && Lift->Call(1),TEXT("call destination station lift")))return;Next(7);return;
    }
    if(Phase==7)
    {
        if(Lift->IsMoving())return;
        if(!Walk(Lift->CarPosition()+FVector(0,0,91)))return;
        if(!Check(Lift->Ride(P,0),TEXT("walk into cabin and descend to the city")))return;Next(8);return;
    }
    if(Phase==8)
    {
        if(Lift->IsMoving())return;
        if(!Walk(T->LiftEntry(1)))return;
        if(!Check(Move->IsMovingOnGround() && P->FallRecoveries==0,TEXT("walk out onto destination city gallery without recovery")))return;
        Capture(TEXT("train-station-access"));Next(9);return;
    }
    if(Phase==9 && Age>1)
    {
        const auto R=M->RecipeAt({3,0});if(!R)return;
        const FVector Exit=T->LiftEntry(1),Sanctum=M->ToRender({3,0},R->Hub+FVector(200,200,91));
        const FVector Tour[]={Exit+FVector(0,400,0),Exit+FVector(500,400,0),Sanctum,
            Exit+FVector(500,400,0),Exit+FVector(0,400,0),Exit};
        if(TourPoint<UE_ARRAY_COUNT(Tour))
        {
            if(!Walk(Tour[TourPoint]))return;
            if(!Check(Move->IsMovingOnGround() && P->FallRecoveries==0,FString::Printf(TEXT("physical station-to-sanctuary path %d"),TourPoint)))return;
            if(TourPoint==2)
            {
                G->RecordNearest();if(!Check(G->IsRecorded(R->Places[0]),TEXT("water-city discovery accessible on foot after alighting")))return;
                Capture(TEXT("water-city-arrival"));
            }
            ++TourPoint;return;
        }
        if(!Walk(Lift->CarPosition()+FVector(0,0,91)))return;
        if(!Check(Lift->Ride(P,1),TEXT("return by station lift")))return;Next(10);return;
    }
    if(Phase==10)
    {
        if(Lift->IsMoving())return;
        if(!Walk(T->BoardingPosition(1)+FVector(1120,0,0)))return;
        if(T->AtStation()!=1 || T->Evidence()->GetNumberField(TEXT("door_open"))<.99)return;
        G->Interact();if(!Check(T->IsRider(P) && T->Evidence()->GetNumberField(TEXT("rider_car"))==1,TEXT("board return service through second car door")))return;Next(11);return;
    }
    if(Phase==11 && T->AtStation()==0 && T->Evidence()->GetNumberField(TEXT("door_open"))>.99)
    {
        G->Interact();if(!Check(!P->IsSeated() && P->FallRecoveries==0 && T->Boardings==2 && T->Alightings==2 && M->RebaseCount>=6,TEXT("656 metre round trip and six origin shifts complete without a fall recovery")))return;
        EW::PlaceBookmark Save;const auto At=M->CurrentPosition();
        if(!Check(G->SaveNow() && G->Store()->LoadCurrent(Save) && Save.Coord==EW::ChunkCoord{0,0} && Save.LocalPosition.Equals(At.LocalPosition,1),TEXT("final save returns to clock plaza station")))return;
        Next(12);return;
    }
    if(Phase==12 && Age>1){Finish();}
}
