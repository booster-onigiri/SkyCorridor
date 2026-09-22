#include "EWUpperRailAudit.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWSkyrail.h"
#include "EWSkyrailPlan.h"
#include "EWHotelPlan.h"
#include "EWSkyTheatrePlan.h"
#include "EWAeroYachtPlan.h"
#include "EWLift.h"
#include "EWMediaScreen.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Algo/Reverse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

AEWUpperRailAudit::AEWUpperRailAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWUpperRailAudit::BeginPlay()
{Super::BeginPlay();Started=Stage=Progress=FPlatformTime::Seconds();FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);if(Report.IsEmpty()){Done=true;FPlatformMisc::RequestExit(false);}}
void AEWUpperRailAudit::BuildSteps()
{
    auto* G=GetGameInstance<UEWGameInstance>();const auto R=G->Manager->RecipeAt({0,0}),East=G->Manager->RecipeAt({1,0});
    auto Walk=[&](const FString& N,TArray<FVector> Path,bool Reverse=false){if(Reverse)Algo::Reverse(Path);Steps.Add({0,0,0,N,{},Path});};
    auto Lift=[&](const FString& Id,int Stop,bool Ride){Steps.Add({Ride?2:1,Stop,0,(Ride?TEXT("ride "):TEXT("call "))+Id+FString::Printf(TEXT("/%d"),Stop),Id,{}});};
    auto Train=[&](int From,int To){Steps.Add({3,From,To,FString::Printf(TEXT("upper train %d to %d"),From,To),{}, {}});};
    auto Shot=[&](FString N){Steps.Add({4,0,0,N,{}, {}});};
    Walk(TEXT("clock plaza platform to transfer lift"),{FVector(11380,6820,3150)});Lift(TEXT("skyrail78/0"),1,false);Lift(TEXT("skyrail78/0"),2,true);
    Walk(TEXT("upper-line transfer platform"),{EWSkyrailPlan::Boarding(1,1)-FVector(0,0,91)});Train(1,0);
    Walk(TEXT("hotel platform to guest lift"),{FVector(6380,6820,30000)});Lift(TEXT("skyrail88/hotel"),2,false);Lift(TEXT("skyrail88/hotel"),0,true);
    for(int I:{0,1,4,5,2,3,6,7})
    {
        if(I==2)Lift(TEXT("skyrail88/hotel"),1,true);
        auto Path=EWSkyrailPlan::HotelPath(*R,I);const auto* Room=EWHotelPlan::Find(*R,I);
        Path.Add(Room->Frame.TransformPosition(FVector(0,-300,0)));
        Walk(FString::Printf(TEXT("walk into suite %d"),101+I),Path);Shot(FString::Printf(TEXT("arrival-suite-%d"),101+I));
        Walk(FString::Printf(TEXT("walk back from suite %d"),101+I),Path,true);
    }
    Lift(TEXT("skyrail88/hotel"),2,true);Walk(TEXT("hotel return platform"),{EWSkyrailPlan::Boarding(0,1)-FVector(0,0,91)});Train(0,1);
    Walk(TEXT("theatre platform to express lift"),{FVector(11380,6820,30000)});Lift(TEXT("skyrail78/0"),2,false);Lift(TEXT("skyrail78/0"),3,true);
    auto Theatre=EWSkyrailPlan::TheatrePath(*R);Walk(TEXT("theatre entrance and front aisle"),Theatre);Shot(TEXT("arrival-theatre"));
    FTransform T;EWSkyTheatrePlan::Frame(*R,T);const auto Seat=EWSkyTheatrePlan::Seats()[4];
    Walk(TEXT("theatre front seat"),{T.TransformPosition(Seat.Stand-FVector(0,0,91))});Steps.Add({5,0,0,TEXT("theatre seating interaction"),{}, {}});
    Walk(TEXT("theatre return to station lift"),Theatre,true);Lift(TEXT("skyrail78/0"),2,true);
    Walk(TEXT("theatre platform for airport"),{EWSkyrailPlan::Boarding(1,1)-FVector(0,0,91)});Train(1,2);
    TArray<FVector> Platform={FVector(24000,6820,30000),FVector(24180,6820,30000),FVector(24180,5400,30000)};
    Walk(TEXT("airport platform around the buffer stop"),Platform);Lift(TEXT("skyrail88/airport"),0,false);Lift(TEXT("skyrail88/airport"),1,true);
    const auto Airport=EWSkyrailPlan::AirportPath(*East);Walk(TEXT("skyport entrance and boarding hall"),Airport);Shot(TEXT("arrival-airport"));
    Walk(TEXT("airport return to station lift"),Airport,true);Lift(TEXT("skyrail88/airport"),0,true);Walk(TEXT("airport return platform"),Platform,true);
    Walk(TEXT("airport train door"),{EWSkyrailPlan::Boarding(2,1)-FVector(0,0,91)});Train(2,1);
    Walk(TEXT("return transfer lift"),{FVector(11380,6820,30000)});Lift(TEXT("skyrail78/0"),2,false);Lift(TEXT("skyrail78/0"),1,true);
    Walk(TEXT("return to clock plaza platform"),{EWSkyrailPlan::Boarding(0)-FVector(0,0,91)});Shot(TEXT("upper-line-complete"));
}
void AEWUpperRailAudit::Next()
{
    if(Index>=0){auto C=MakeShared<FJsonObject>();C->SetBoolField(TEXT("pass"),true);C->SetStringField(TEXT("check"),Steps[Index].Name);Checks.Add(MakeShared<FJsonValueObject>(C));}
    ++Index;Sub=Point=0;Stage=Progress=FPlatformTime::Seconds();Best=DBL_MAX;
    if(Index>=Steps.Num()){Finish();return;}
    auto C=MakeShared<FJsonObject>();C->SetNumberField(TEXT("step"),Index);C->SetStringField(TEXT("name"),Steps[Index].Name);C->SetNumberField(TEXT("seconds"),Stage-Started);
    FString Text;FJsonSerializer::Serialize(C,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Report+TEXT(".progress.json")));
}
void AEWUpperRailAudit::Finish(const FString& Error)
{
    if(Done)return;Done=true;auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(P)P->SetTestMovement(FVector::ZeroVector,false);
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty() && P && P->FallRecoveries==0 && G->SaveNow());
    O->SetStringField(TEXT("failure"),Error);O->SetArrayField(TEXT("checks"),Checks);O->SetNumberField(TEXT("walked_metres"),Walked/100.);
    O->SetNumberField(TEXT("fall_recoveries"),P?P->FallRecoveries:-1);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    if(P)O->SetStringField(TEXT("position"),P->GetActorLocation().ToString());if(G->SkyrailUpper)O->SetObjectField(TEXT("upper_train"),G->SkyrailUpper->Evidence());
    O->SetNumberField(TEXT("origin_rebases"),G->Manager->RebaseCount);O->SetStringField(TEXT("method"),TEXT("real CharacterMovement, trains, lift rides, eight hotel interiors, theatre seat and skyport; only initial station travel; isolated data and no sound"));
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);FPlatformMisc::RequestExit(false);
}
void AEWUpperRailAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;const double Now=FPlatformTime::Seconds();
    if(Now-Started>1200 || Now-Stage>140){Finish(FString::Printf(TEXT("timeout step %d"),Index));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    auto* M=G?G->Manager.Get():nullptr;if(!M || !P || !PC || !G->SkyrailUpper || M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt(M->PlayerCoord()))return;
    auto* Move=P->GetCharacterMovement();
    if(Index==-2){G->VisitSkyrail();Index=-1;Stage=Now;return;}
    if(Index==-1){if(!Move->IsMovingOnGround() || !M->ReadyAt({1,0}) || Now-Stage<3)return;Last=P->GetActorLocation();BuildSteps();Next();return;}
    const auto S=Steps[Index];const double Segment=FVector::Dist2D(P->GetActorLocation(),Last);
    if((S.Kind==0 || (S.Kind==2 && Sub==0)) && Move->IsMovingOnGround() && Segment<100)Walked+=Segment;
    Last=P->GetActorLocation();
    auto Walk=[&](FVector Global)->bool
    {
        const auto Goal=M->ToRender({0,0},Global)+FVector(0,0,91),D=Goal-P->GetActorLocation();const double Dist=D.Size2D();
        if(Dist<23){P->SetTestMovement(FVector::ZeroVector,false);Best=DBL_MAX;Progress=Now;
            if(!Move->IsMovingOnGround() || P->FallRecoveries>0 || FMath::Abs(D.Z)>150){Finish(TEXT("unsupported destination ")+S.Name+TEXT(" goal ")+Goal.ToString());return false;}return true;}
        if(Dist<Best-3){Best=Dist;Progress=Now;}else if(Now-Progress>8){Finish(TEXT("walk blocked ")+S.Name+FString::Printf(TEXT(" point %d at "),Point)+P->GetActorLocation().ToString()+TEXT(" goal ")+Goal.ToString());return false;}
        P->SetTestMovement(D.GetSafeNormal2D(),true);PC->SetControlRotation(D.Rotation());return false;
    };
    if(S.Kind==0){if(Point<S.Path.Num() && Walk(S.Path[Point]))++Point;if(Point==S.Path.Num())Next();return;}
    if(S.Kind==1 || S.Kind==2)
    {
        AEWLift* L=nullptr;for(TActorIterator<AEWLift> It(GetWorld());It;++It)if(It->Spec.Id==S.Id){L=*It;break;}if(!L){Finish(TEXT("lift missing ")+S.Id);return;}
        if(Sub==0)
        {
            if(L->IsMoving())return;
            if(S.Kind==2){if(!Walk(M->ToLocal({0,0},L->CarPosition())))return;if(!L->Ride(P,S.A)){Finish(TEXT("cannot board ")+S.Name);return;}}
            else if(!L->Call(S.A)){Finish(TEXT("cannot call ")+S.Name);return;}
            Sub=1;return;
        }
        if(!L->IsMoving() && L->FloorIndex()==S.A){Last=P->GetActorLocation();Next();}return;
    }
    if(S.Kind==3)
    {
        auto* T=G->SkyrailUpper.Get();if(Sub==0)
        {if(T->AtStation()!=S.A || T->Destination()!=S.B || T->Evidence()->GetNumberField(TEXT("door_open"))<.99)return;G->Interact();if(!T->IsRider(P)){Finish(TEXT("boarding failed ")+S.Name);return;}Sub=1;return;}
        if(Sub==1){if(T->AtStation()!=S.B || T->Evidence()->GetNumberField(TEXT("door_open"))<.99)return;G->Interact();if(T->IsRider(P)){Finish(TEXT("alighting failed ")+S.Name);return;}Sub=2;return;}
        if(Move->IsMovingOnGround())Next();return;
    }
    if(S.Kind==4){if(Sub==0){FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/S.Name+TEXT(".png"),false,false);Sub=1;Stage=Now;}else if(Now-Stage>1)Next();return;}
    if(S.Kind==5){if(Sub==0){G->Interact();if(!P->IsSeated()){Finish(TEXT("theatre seating failed"));return;}Sub=1;Stage=Now;}else if(Now-Stage>1){if(!P->LeaveSeat()){Finish(TEXT("theatre exit failed"));return;}Next();}}
}
