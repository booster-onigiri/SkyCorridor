#include "EWPoolAudit.h"
#include "EWWaterView.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWDayCycle.h"
#include "EWLift.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

AEWPoolAudit::AEWPoolAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWPoolAudit::BeginPlay()
{Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);Resume=FParse::Param(FCommandLine::Get(),TEXT("EWPoolResume"));}
void AEWPoolAudit::Capture(const FString& Name){FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/Name+TEXT(".png"),false,false);}
void AEWPoolAudit::Next(int32 Value)
{Phase=Value;Stage=FPlatformTime::Seconds();FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"phase\":%d,\"pool\":%d,\"leg\":%d}"),Phase,Index,Leg),*(Report+TEXT(".progress.json")));}
bool AEWPoolAudit::Check(bool Pass,const FString& Name)
{auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("pass"),Pass);O->SetStringField(TEXT("check"),Name);Checks.Add(MakeShared<FJsonValueObject>(O));if(!Pass)Finish(Name);return Pass;}
void AEWPoolAudit::Finish(const FString& Error)
{
    if(Done)return;Done=true;auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(P)P->SetTestMovement(FVector::ZeroVector,false);
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);O->SetArrayField(TEXT("checks"),Checks);
    O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);O->SetBoolField(TEXT("resume"),Resume);O->SetNumberField(TEXT("walked_cm"),Walked);
    if(P)O->SetNumberField(TEXT("fall_recoveries"),P->FallRecoveries);if(G && G->Manager)O->SetNumberField(TEXT("rebases"),G->Manager->RebaseCount);
    if(G && G->WaterView)O->SetObjectField(TEXT("water"),G->WaterView->Evidence());
    O->SetStringField(TEXT("scope"),TEXT("Silent isolated character-physics traversal and camera-water detection. Travel between rooftop fixtures; no physical keyboard, speaker or remote network acceptance."));
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);FPlatformMisc::RequestExit(false);
}
void AEWPoolAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;const double Age=FPlatformTime::Seconds()-Stage;
    if(FPlatformTime::Seconds()-Started>420 || Age>75){Capture(TEXT("failure"));Finish(FString::Printf(TEXT("timeout phase %d pool %d leg %d"),Phase,Index,Leg));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(!G || !G->Manager || !G->WaterView || !P)return;
    auto* M=G->Manager.Get();auto* W=G->WaterView.Get();auto* PC=UGameplayStatics::GetPlayerController(this,0);auto* Move=P->GetCharacterMovement();
    if(M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt(M->PlayerCoord()) || W->Pools().IsEmpty())return;
    FCollisionQueryParams Q;Q.AddIgnoredActor(P);
    auto Trace=[&](FVector A,FVector B,FHitResult& Hit){return GetWorld()->LineTraceSingleByChannel(Hit,A,B,ECC_Visibility,Q);};
    auto Walk=[&](const FVector& Target)
    {
        const auto Here=P->GetActorLocation();if(!Last.IsZero()){const double D=FVector::Dist2D(Last,Here);if(D<100)Walked+=D;}Last=Here;
        const auto D=Target-Here;PC->SetControlRotation(FRotator(-8,D.Rotation().Yaw,0));
        if(D.Size2D()>30){P->SetTestMovement(D,true);return false;}P->SetTestMovement(FVector::ZeroVector,false);return true;
    };
    if(Phase==0)
    {
        if(!Check(W->Pools().Num()==3 && W->Evidence()->GetBoolField(TEXT("material_loaded")),TEXT("three rooftop pools and underwater material loaded")))return;
        if(Resume){G->ContinueWorld();Next(80);return;}
        const auto R=EW::GenerateChunk(M->Descriptor(),{0,0});const auto* Roof=R.Parts.FindByPredicate([](const EW::Part& V){return V.Mesh.ToString().StartsWith(TEXT("UrbanGarden_"));});
        if(!Check(Roof!=nullptr,TEXT("original rooftop garden fixture exists")))return;Garden=Roof->Transform;
        auto B=M->CurrentPosition();B.Coord={0,0};B.LocalPosition=Garden.TransformPosition(FVector(-650,1700,100));B.Yaw=Garden.Rotator().Yaw;G->Visit(B);Next(1);return;
    }
    if(Phase==1 && Age>1.5 && Move->IsMovingOnGround())
    {
        auto T=[&](FVector A){return M->ToRender({0,0},Garden.TransformPosition(A));};FHitResult Hit;
        if(!Check(Trace(T(FVector(-1450,-500,160)),T(FVector(-950,-500,160)),Hit),TEXT("visible pavilion column blocks at imported negative Y")))return;
        if(!Check(!Trace(T(FVector(-650,1700,160)),T(FVector(650,1700,160)),Hit),TEXT("former invisible pavilion column is clear")))return;
        if(!Check(Trace(T(FVector(1250,-1100,180)),T(FVector(1250,-1100,0)),Hit) && FMath::Abs(Garden.InverseTransformPosition(M->ToLocal({0,0},Hit.ImpactPoint)).Z-110)<2,TEXT("tree planter top matches visible 110 cm box")))return;
        if(!Check(!Trace(T(FVector(1100,1100,180)),T(FVector(1100,1100,15)),Hit),TEXT("former mirrored tree planter is clear")))return;
        if(!Check(Trace(T(FVector(-600,-1100,900)),T(FVector(-600,-1100,530)),Hit) && FMath::Abs(Garden.InverseTransformPosition(M->ToLocal({0,0},Hit.ImpactPoint)).Z-735)<2,TEXT("curved canopy centre has matching upper collision")))return;
        Last=P->GetActorLocation();Next(2);return;
    }
    if(Phase==2)
    {
        if(!Walk(M->ToRender({0,0},Garden.TransformPosition(FVector(650,1700,90)))))return;
        if(!Check(Move->IsMovingOnGround() && P->FallRecoveries==0,TEXT("character walks through previously blocked roof space")))return;
        Capture(TEXT("roof-collision-fixed"));Next(3);return;
    }
    if(Phase==3 && Age>1){W->VisitPool(Index);Next(10);return;}
    if(Phase==10 && Age>2 && Move->IsMovingOnGround())
    {
        const auto& Site=W->Pools()[Index];auto T=[&](FVector A){return M->ToRender(Site.Coord,Site.Frame.TransformPosition(A));};FHitResult Hit;
        if(!Check(Trace(T(FVector(100,0,400)),T(FVector(100,0,-20)),Hit) && FMath::Abs(Site.Frame.InverseTransformPosition(M->ToLocal(Site.Coord,Hit.ImpactPoint)).Z-EWPoolPlan::Bottom)<2,TEXT("water has no invisible walkable surface; basin floor matches ")+Site.Name))return;
        if(!Check(Trace(T(FVector(0,0,140)),T(FVector(880,0,140)),Hit),TEXT("glazed end wall retains the player ")+Site.Name))return;
        FEWWaterSample S;
        if(!Check(W->Sample(T(FVector(0,0,180)),S) && !W->Sample(T(FVector(0,0,245)),S) && !W->Sample(T(FVector(950,0,180)),S),TEXT("water uses camera depth and bounded sides ")+Site.Name))return;
        const auto R=M->RecipeAt(Site.Coord);const FString Id=FString::Printf(TEXT("%s/%d"),*Site.Coord.Text(),Site.Column);
        const auto* Lift=R->Lifts.FindByPredicate([&](const auto& L){return L.Id==Id;});
        if(!Check(Lift && Lift->Stops.ContainsByPredicate([&](const auto& S){return S.Label.Contains(Site.Name);}),TEXT("pool is named at its existing rooftop lift ")+Site.Name))return;
        if(G->DayCycle)G->DayCycle->SetAuditHour(Index==0?12:Index==1?8:21);
        Leg=0;Last=P->GetActorLocation();Next(11);return;
    }
    if(Phase==11)
    {
        const auto& Site=W->Pools()[Index];
        const FVector Route[]={FVector(-850,0,240),FVector(-300,0,18),FVector(500,0,18),FVector(0,220,18),FVector(-310,0,18),FVector(-850,0,240),FVector(-1490,0,0)};
        const FVector Target=M->ToRender(Site.Coord,Site.Frame.TransformPosition(Route[Leg])+FVector(0,0,90));
        if(!Walk(Target))return;
        if(!Move->IsMovingOnGround())return;
        if(!Check(FMath::Abs(P->GetActorLocation().Z-Target.Z)<32 && P->FallRecoveries==0,FString::Printf(TEXT("walked pool %d stair/basin route leg %d"),Index,Leg)))return;
        if(Leg==2){Next(12);return;}
        if(Leg==6){Next(13);return;}
        ++Leg;Next(11);return;
    }
    if(Phase==12 && Age>1)
    {
        if(!Check(W->Evidence()->GetNumberField(TEXT("weight"))>.99 && W->Evidence()->GetNumberField(TEXT("camera_depth_cm"))>30,TEXT("real player camera submerges and applies optics ")+FString::FromInt(Index)))return;
        if(!Check(G->DayCycle && FMath::Abs(G->DayCycle->Evidence()->GetNumberField(TEXT("game_hour"))-(Index==0?12:Index==1?8:21))<.01,TEXT("requested day/morning/night hour is active ")+FString::FromInt(Index)))return;
        PC->SetControlRotation(FRotator(-6,W->Pools()[Index].Frame.Rotator().Yaw+180,0));Next(14);return;
    }
    if(Phase==14 && Age>1){Capture(FString::Printf(TEXT("pool-%d-underwater"),Index));++Leg;Next(11);return;}
    if(Phase==13 && Age>1)
    {
        if(!Check(W->Evidence()->GetNumberField(TEXT("weight"))==0 && Move->IsMovingOnGround(),TEXT("leaving water restores normal view ")+FString::FromInt(Index)))return;
        PC->SetControlRotation(FRotator(5,W->Pools()[Index].Frame.Rotator().Yaw,0));Next(15);return;
    }
    if(Phase==15 && Age>1){Capture(FString::Printf(TEXT("pool-%d-returned"),Index));Next(16);return;}
    if(Phase==16 && Age>1)
    {
        if(++Index<3){W->VisitPool(Index);Next(10);return;}
        if(!Check(G->SaveNow(),TEXT("roof location saved in independent fixture")))return;Finish();return;
    }
    if(Phase==80 && Age>2 && Move->IsMovingOnGround())
    {
        const auto B=M->CurrentPosition();const auto Expected=EWPoolPlan::Arrival(W->Pools()[2]);
        if(!Check(B.Coord==Expected.Coord && FVector::Dist2D(B.LocalPosition,Expected.LocalPosition)<80 && W->Evidence()->GetNumberField(TEXT("weight"))==0 && P->FallRecoveries==0,TEXT("extracted launcher restores saved rooftop beside pool with clear view")))return;
        Capture(TEXT("rooftop-restored"));Next(81);return;
    }
    if(Phase==81 && Age>1)Finish();
}
