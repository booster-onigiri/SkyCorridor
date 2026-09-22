#include "EWAeroAudit.h"
#include "EWAeroYacht.h"
#include "EWAeroYachtPlan.h"
#include "EWAirship.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWGameInstance.h"
#include "EWDayCycle.h"
#include "EWLift.h"
#include "EngineUtils.h"
#include "EWMediaScreen.h"
#include "EWBrowserSurface.h"
#include "EWSkyTheatrePlan.h"
#include "EWAero87Data.inl"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "RHI.h"

AEWAeroAudit::AEWAeroAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWAeroAudit::BeginPlay()
{Super::BeginPlay();Started=Stage=Progress=FPlatformTime::Seconds();FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);Resume=FParse::Param(FCommandLine::Get(),TEXT("EWAero87Resume"));if(Resume)Phase=-1;if(Report.IsEmpty() || IFileManager::Get().FileExists(*Report))Finish(TEXT("fresh explicit report required"));}
bool AEWAeroAudit::Check(bool Pass,const FString& Name)
{auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("check"),Name);C->SetBoolField(TEXT("pass"),Pass);Checks.Add(MakeShared<FJsonValueObject>(C));if(!Pass)Finish(Name);return Pass;}
void AEWAeroAudit::Next(int32 Value)
{Phase=Value;Stage=Progress=FPlatformTime::Seconds();Best=DBL_MAX;Point=0;UE_LOG(LogTemp,Display,TEXT("AERO87_STAGE %d"),Phase);}
void AEWAeroAudit::Capture(const FString& Name)
{if(!GUsingNullRHI)FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/Name+TEXT(".png"),false,false);}
void AEWAeroAudit::Clock(double Value,bool Run)
{if(auto* G=GetGameInstance<UEWGameInstance>())for(auto Y:G->Yachts){Y->AuditRun(false);Y->AuditTime(Value);Y->AuditRun(Run);}}
void AEWAeroAudit::Finish(const FString& Error)
{
    if(Done)return;Done=true;auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);
    O->SetArrayField(TEXT("checks"),Checks);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);O->SetNumberField(TEXT("walk_metres"),PathMetres);
    O->SetNumberField(TEXT("flight_max_local_drift_cm"),FlightMaxDrift);O->SetBoolField(TEXT("resume"),Resume);
    if(auto* G=GetGameInstance<UEWGameInstance>())
    {
        if(G->Manager)O->SetNumberField(TEXT("rebases"),G->Manager->RebaseCount-Rebases);
        if(G->Skyport)O->SetObjectField(TEXT("port"),G->Skyport->Evidence());
        TArray<TSharedPtr<FJsonValue>> Ships;for(const auto Y:G->Yachts)Ships.Add(MakeShared<FJsonValueObject>(Y->Evidence()));O->SetArrayField(TEXT("yachts"),Ships);
        O->SetNumberField(TEXT("records_before"),Records);O->SetNumberField(TEXT("records_after"),G->RecordCount());
    }
    if(auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))
    {P->SetTestMovement(FVector::ZeroVector,false);O->SetNumberField(TEXT("fall_recoveries"),P->FallRecoveries);O->SetStringField(TEXT("player"),P->GetActorLocation().ToString());}
    O->SetStringField(TEXT("scope"),TEXT("real collision, normal walking and moving-base physics; isolated saves; physical audio disabled; online external network not measured"));
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));if(!Report.IsEmpty())FFileHelper::SaveStringToFile(Text,*Report);FPlatformMisc::RequestExit(false);
}
bool AEWAeroAudit::Walk(const FVector& Local,bool OnShip,double Timeout)
{
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    const FTransform T=OnShip?G->Yachts[ShipIndex]->GetActorTransform():FTransform(G->Manager->ToRender({0,0},EWAeroYachtPlan::Design().Terminal));
    const FVector Goal=T.TransformPosition(Local);const FVector Now=P->GetActorLocation();const double Distance=FVector::Dist2D(Goal,Now);
    if(!LastPoint.IsNearlyZero()){const double Step=FVector::Dist(Now,LastPoint);if(Step<100)PathMetres+=Step/100.;}LastPoint=Now;
    if(Distance<Best-3){Best=Distance;Progress=FPlatformTime::Seconds();}
    if(FPlatformTime::Seconds()-Progress>Timeout){Finish(FString::Printf(TEXT("walking stalled phase=%d point=%d distance=%.1f height=%.1f local=%s"),Phase,Point,Distance,Now.Z-Goal.Z,*T.InverseTransformPosition(Now).ToString()));return false;}
    if(Distance>28){P->SetTestMovement(Goal-Now,true);return false;}
    P->SetTestMovement(FVector::ZeroVector,false);
    if(!Check(FMath::Abs(Now.Z-Goal.Z)<45 && P->GetCharacterMovement()->IsMovingOnGround() && P->FallRecoveries==0,
        FString::Printf(TEXT("walked phase %d waypoint %d with real floor, no jump or recovery"),Phase,Point)))return false;
    Best=DBL_MAX;Progress=FPlatformTime::Seconds();++Point;return true;
}
void AEWAeroAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;const double Age=FPlatformTime::Seconds()-Stage;
    if(FPlatformTime::Seconds()-Started>850){Finish(FString::Printf(TEXT("timeout at phase %d"),Phase));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !G->Manager || !P || !PC || G->Yachts.Num()!=2)return;auto* M=G->Manager.Get();
    if(M->IsTravelling() || P->IsStreamingHeld())return;
    // A normal launch first shows the title's preview world. Exercise the
    // same Continue action as the player before checking the saved position.
    if(Phase==-1 && P->GetCharacterMovement()->IsMovingOnGround())
    {G->ContinueWorld();Next(0);return;}
    if(Phase==0)
    {
        Records=G->RecordCount();Rebases=M->RebaseCount;Clock(10);
        if(Resume)
        {
            const auto B=M->CurrentPosition();const auto Dock=EWAeroYachtPlan::Arrival();
            if(!Check(B.Coord==Dock.Coord && FVector::Dist2D(B.LocalPosition,Dock.LocalPosition)<60 && FMath::Abs(B.LocalPosition.Z-Dock.LocalPosition.Z)<45,
                TEXT("new process resumes moving-ship save on the reachable rooftop port")))return;
            if(!Check(P->GetCharacterMovement()->IsMovingOnGround() && P->FallRecoveries==0,TEXT("resumed player has physical floor, no aerial fallback")))return;
            Capture(TEXT("port-resumed"));Next(90);return;
        }
        if(!Check(G->Airships.Num()==3 && G->Yachts.Num()==2,TEXT("three raised scenic airships plus two boardable luxury yachts")))return;
        for(const auto Y:G->Yachts)
        {
            if(!Check(Y->Ready() && Y->Evidence()->GetNumberField(TEXT("loaded_meshes"))==8 &&
                Y->Evidence()->GetNumberField(TEXT("convex_bodies"))==EWAero87Data::Bodies().Num(),TEXT("authored hull meshes and all compound collision bodies imported")))return;
            if(!Check(Y->Evidence()->GetNumberField(TEXT("minimum_roof_clearance_cm"))>=400,TEXT("entire lower hull clears the highest roof and floating theatre")))return;
        }
        for(const auto A:G->Airships)if(!Check(A->Evidence()->GetNumberField(TEXT("cruise_z"))-565>EWAeroYachtPlan::Design().Ship.Z+2000,TEXT("scenic ship flight band clears luxury deck and theatre")))return;
        double Separation=DBL_MAX;
        for(double T=0;T<300;T+=.25)Separation=FMath::Min(Separation,FVector::Dist(EWAeroYachtPlan::Pose(T,0).GetLocation(),EWAeroYachtPlan::Pose(T,1).GetLocation()));
        if(!Check(Separation>14500,TEXT("two ships remain separated by more than the complete hull diameter throughout the loop")))return;
        for(const EW::ChunkCoord C:{EW::ChunkCoord{0,0},EW::ChunkCoord{1,0}})
        {
            const auto R=EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(),C);FString Error;
            if(!Check(!R.bFallback && R.Validate(Error),TEXT("theatre and port preserve valid deterministic world recipes")))return;
        }
        G->VisitSkyTheatre();Next(1);return;
    }
    if(Phase==1 && Age>3)
    {
        auto* S=G->SkyTheatre.Get();
        if(!Check(S && S->Available() && S->ScreenWidth()==10000 && S->GetActorRotation().Pitch<-23.9,TEXT("100 m floating screen tilts toward the audience by 24 degrees")))return;
        const auto R=M->RecipeAt({0,0});
        if(!Check(R && !R->Parts.ContainsByPredicate([](const auto& V){return V.Mesh==TEXT("SkyTheatre82Deck");}),TEXT("old screen housing, legs and braces are absent")))return;
        if(!Check(S->SitSeat(27),TEXT("back-row seating remains usable")))return;
        S->LookAtScreen();S->Browser()->TestSyncFilm();G->DayCycle->SetAuditHour(12);Next(2);return;
    }
    if(Phase==2 && Age>5){Capture(TEXT("sky87-seated-day"));G->SkyTheatre->Stop();P->LeaveSeat();Next(3);return;}
    if(Phase==3 && Age>1){G->VisitSkyport();Clock(10);Next(4);return;}
    if(Phase==4 && Age>3)
    {
        if(!Check(P->GetCharacterMovement()->IsMovingOnGround() && G->Skyport->Evidence()->GetBoolField(TEXT("open")) &&
            G->Skyport->Evidence()->GetNumberField(TEXT("extension"))>.98,TEXT("top-floor port has a lowered boarding bridge and open safety gate")))return;
        Capture(TEXT("skyport-day"));Next(40);return;
    }
    if(Phase==40)
    {
        for(TActorIterator<AEWLift> It(GetWorld());It;++It)if(!It->Spec.Stops.IsEmpty() && It->Spec.Stops.Last().Label.Contains(TEXT("アウレリア空中港"))){PortLift=*It;break;}
        if(!Check(PortLift && PortLift->Call(PortLift->Spec.Stops.Num()-1),TEXT("existing building lift has a callable top-floor skyport stop")))return;
        Next(41);return;
    }
    if(Phase>=41 && Phase<=45)
    {
        if(!PortLift){Finish(TEXT("port lift disappeared during a ride"));return;}
        auto* L=PortLift.Get();if(L->IsMoving())return;
        const int Top=L->Spec.Stops.Num()-1;
        const FVector Port=M->ToRender({0,0},EWAeroYachtPlan::Design().Terminal);
        const auto Local=[&](FVector World){return World-Port+FVector(0,0,99);};
        if(Phase==41)
        {
            const FVector Route[]={FVector(0,0,99),Local(L->GetActorTransform().TransformPosition(L->Spec.Stops[Top].Entry)),Local(L->CarPosition())};
            if(Point<UE_ARRAY_COUNT(Route)){Walk(Route[Point],false);return;}
            if(!Check(L->Ride(P,Top-1),TEXT("walked into the existing lift and rode from skyport down to the old rooftop")))return;Next(42);return;
        }
        if(Phase==42)
        {
            if(!Check(!P->IsLiftRiding() && L->FloorIndex()==Top-1,TEXT("lift releases its passenger on the original rooftop")))return;
            Next(43);return;
        }
        if(Phase==43)
        {
            const FVector Route[]={Local(L->LandingPosition(Top-1)),Local(L->CarPosition())};
            if(Point<UE_ARRAY_COUNT(Route)){Walk(Route[Point],false);return;}
            if(!Check(L->Ride(P,Top),TEXT("original rooftop passengers can board and select the new skyport floor")))return;Next(44);return;
        }
        if(Phase==44)
        {
            if(!Check(!P->IsLiftRiding() && L->FloorIndex()==Top,TEXT("lift reaches the new upper terminal with its passenger")))return;
            Next(45);return;
        }
        const FVector Route[]={Local(L->GetActorTransform().TransformPosition(L->Spec.Stops[Top].Entry)),FVector(0,0,99),FVector(0,600,99)};
        if(Point<UE_ARRAY_COUNT(Route)){Walk(Route[Point],false);return;}
        Next(5);return;
    }
    if(Phase==5)
    {
        const FVector Route[]={FVector(0,2200,99),FVector(0,3300,99),FVector(0,3920,99)};
        if(Point<UE_ARRAY_COUNT(Route)){Walk(Route[Point],false);return;}
        if(!Check(G->Yachts[0]->Contains(P) && P->GetMovementBase()==G->Yachts[0]->WalkingBody(),TEXT("normal walking crosses the bridge and establishes the ship as a moving base")))return;
        Next(6);return;
    }
    if(Phase==6)
    {
        const FVector Route[]={FVector(-3300,-1680,99),FVector(-3350,-1350,99),FVector(-950,-1350,699),FVector(-950,-800,699),FVector(2400,-800,699),FVector(3500,0,699)};
        if(Point<UE_ARRAY_COUNT(Route)){Walk(Route[Point],true);return;}
        PC->SetControlRotation((G->Yachts[0]->GetActorTransform().TransformPosition(FVector(0,0,640))-P->Camera->GetComponentLocation()).Rotation());
        Next(67);return;
    }
    if(Phase==67 && Age>1){Capture(TEXT("yacht-deck-walked"));Next(68);return;}
    if(Phase==68 && Age>1){Next(7);return;}
    if(Phase==7)
    {
        const FVector Goal=G->Yachts[0]->GetActorTransform().TransformPosition(FVector(3500,2200,699));P->SetTestMovement(Goal-P->GetActorLocation(),true);
        if(Age<6)return;P->SetTestMovement(FVector::ZeroVector,false);
        const FVector L=G->Yachts[0]->GetActorTransform().InverseTransformPosition(P->GetActorLocation());
        if(!Check(L.Y>1150 && L.Y<1510 && P->FallRecoveries==0,TEXT("glass observation railing physically stops a sustained outward walk")))return;Next(8);return;
    }
    if(Phase==8)
    {
        const FVector Route[]={FVector(3500,0,699),FVector(2900,0,699),FVector(2200,0,592),FVector(2900,0,699),FVector(3500,0,699)};
        if(Point<UE_ARRAY_COUNT(Route)){Walk(Route[Point],true);return;}
        FlightAnchor=G->Yachts[0]->GetActorTransform().InverseTransformPosition(P->GetActorLocation());Clock(55,true);Next(9);return;
    }
    if(Phase==9)
    {
        auto* Y=G->Yachts[0].Get();const auto L=Y->GetActorTransform().InverseTransformPosition(P->GetActorLocation());
        FlightMaxDrift=FMath::Max(FlightMaxDrift,FVector::Dist2D(L,FlightAnchor));
        if(FlightMaxDrift>90 || P->FallRecoveries>0){Finish(TEXT("passenger lost moving deck or drifted during flight / rebasing"));return;}
        if(Y->Clock()<301)return;
        Clock(307);
        if(!Check(FlightMaxDrift<90 && M->RebaseCount>Rebases && P->GetMovementBase()==Y->WalkingBody(),TEXT("full 245-second circuit carries a standing passenger through chunk boundaries and origin changes")))return;
        if(!Check(Y->AtDock() && Y->CanBoard(),TEXT("vessel returns to the same port and reopens boarding")))return;
        Next(10);return;
    }
    if(Phase==10)
    {
        const FVector Route[]={FVector(2400,-800,699),FVector(-950,-800,699),FVector(-950,-1350,699),FVector(-3350,-1350,99),FVector(-3350,-1680,99),FVector(-2400,-1680,99)};
        if(Point<UE_ARRAY_COUNT(Route)){Walk(Route[Point],true);return;}Next(11);return;
    }
    if(Phase==11)
    {
        const FVector Route[]={FVector(0,3280,99),FVector(0,2200,99),FVector(0,1000,99)};
        if(Point<UE_ARRAY_COUNT(Route)){Walk(Route[Point],false);return;}
        if(!Check(!G->Yachts[0]->Contains(P) && P->GetMovementBase()!=G->Yachts[0]->WalkingBody(),TEXT("passenger walks down both decks and off the returning ship onto the building")))return;
        Clock(160);ShipIndex=1;Next(12);return;
    }
    if(Phase==12 && Age>3)
    {
        const FVector Route[]={FVector(0,2200,99),FVector(0,3300,99),FVector(0,3920,99)};
        if(Point<UE_ARRAY_COUNT(Route)){Walk(Route[Point],false);return;}
        if(!Check(G->Yachts[1]->Contains(P) && P->GetMovementBase()==G->Yachts[1]->WalkingBody(),TEXT("the second independent ship is also boardable")))return;Next(13);return;
    }
    if(Phase==13)
    {
        const FVector Route[]={FVector(-3400,-1680,99),FVector(-3450,0,99),FVector(-3450,1350,99),FVector(-950,1350,699),FVector(-950,800,699),FVector(2400,800,699)};
        if(Point<UE_ARRAY_COUNT(Route)){Walk(Route[Point],true);return;}
        PC->SetControlRotation((G->Yachts[1]->GetActorTransform().TransformPosition(FVector(0,0,700))-P->Camera->GetComponentLocation()).Rotation());
        G->DayCycle->SetAuditHour(21);Next(14);return;
    }
    if(Phase==14 && Age>5){Capture(TEXT("yacht-second-night"));Next(15);return;}
    if(Phase==15 && Age>1)
    {
        Clock(205,true);Next(16);return;
    }
    if(Phase==16 && Age>15)
    {
        if(!Check(!G->Yachts[1]->AtDock() && G->Yachts[1]->Contains(P),TEXT("second ship departs with its passenger aboard")))return;
        const auto B=M->CurrentPosition(),Dock=EWAeroYachtPlan::Arrival();
        if(!Check(B.Coord==Dock.Coord && B.LocalPosition.Equals(Dock.LocalPosition,1),TEXT("save position aboard a moving ship resolves to its safe accessible port")))return;
        if(!Check(G->SaveNow() && G->RecordCount()==Records && P->FallRecoveries==0,TEXT("save succeeds without altering discoveries or triggering a fall recovery")))return;
        Next(90);return;
    }
    if(Phase==90 && Age>2)Finish();
}
