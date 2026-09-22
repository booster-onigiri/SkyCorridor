#include "EWTraversalAudit.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWLift.h"
#include "EWGameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

AEWTraversalAudit::AEWTraversalAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
AEWCharacter* AEWTraversalAudit::Player()const{return Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));}
AEWChunkManager* AEWTraversalAudit::Manager()const{auto* G=GetGameInstance<UEWGameInstance>();return G?G->Manager.Get():nullptr;}
AEWLift* AEWTraversalAudit::Lift(int32 Column)const
{
    const FString Id=FString::Printf(TEXT("0,0/%d"),Column);
    for(TActorIterator<AEWLift> It(GetWorld());It;++It)if(It->Spec.Id==Id)return *It;
    return nullptr;
}
void AEWTraversalAudit::BeginPlay()
{
    Super::BeginPlay();Started=StageStart=ProgressTime=FPlatformTime::Seconds();
    ReportPath=FPaths::ProjectSavedDir()/TEXT("Verification/traversal-")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".json");
    FParse::Value(FCommandLine::Get(),TEXT("EWReport="),ReportPath);StreamPath=FPaths::ChangeExtension(ReportPath,TEXT("jsonl"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath),true);
    bResume=FParse::Param(FCommandLine::Get(),TEXT("EWTraversalResumeOnly"));
    bVertical=FParse::Param(FCommandLine::Get(),TEXT("EWVerticalAccessAudit"));
    FParse::Value(FCommandLine::Get(),TEXT("EWExpectedZ="),ExpectedZ);
    Event(TEXT("start"),TEXT("Actual CharacterMovement and moving lift actor. Inter-case placement is explicit. Physical controllers are a separate test."));
}
void AEWTraversalAudit::Event(const FString& Type,const FString& Text)
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("event"),Type);O->SetStringField(TEXT("detail"),Text);
    O->SetNumberField(TEXT("elapsed"),FPlatformTime::Seconds()-Started);
    if(auto* P=Player()){O->SetStringField(TEXT("position"),P->GetActorLocation().ToString());O->SetBoolField(TEXT("on_ground"),P->GetCharacterMovement()->IsMovingOnGround());}
    FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&S));
    FFileHelper::SaveStringToFile(S+TEXT("\n"),*StreamPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
    UE_LOG(LogTemp,Display,TEXT("EW_TRAVERSAL_%s %s"),*Type.ToUpper(),*Text);
}
void AEWTraversalAudit::BuildCases()
{
    const auto R=Manager()->RecipeAt({0,0});if(!R)return;
    if(bVertical)
    {
        for(int32 Column=0;Column<4;++Column)
        {
            TArray<EW::CityWalkFloor> Floors;
            for(const auto& F:R->CityFloors)if(F.Column==Column)Floors.Add(F);
            const int32 Upper=Floors.Num()-5;
            const FTransform T=Floors[Upper].Transform,Lower=Floors[Upper-1].Transform;
            Cases.Add({FString::Printf(TEXT("jump_down_18m_tower_%d"),Column),
                {T.TransformPosition(FVector(1000,-2150,0)),T.TransformPosition(FVector(1000,-2950,0)),
                 Lower.TransformPosition(FVector(600,-2070,0))},3,Column,T});
            auto* L=Lift(Column);if(!L)continue;
            for(int32 Stop:{0,L->Spec.Stops.Num()/2,L->Spec.Stops.Num()-5,L->Spec.Stops.Num()-1})
            {
                FEWTraversalCase C;C.Name=FString::Printf(TEXT("landing_alignment_%d_stop_%d"),Column,Stop);
                C.Kind=5;C.Column=Column;C.Stop=Stop;
                const auto& S=L->Spec.Stops[Stop];
                C.Points={S.Entry+(S.Entry-S.Threshold).GetSafeNormal2D()*90.,S.Landing};Cases.Add(C);
            }
            if(Column==0)
            {
                const auto B=Floors[0].Transform;
                Cases.Add({TEXT("void_recovers_below_lowest_floor"),
                    {B.TransformPosition(FVector(1000,-2150,0)),B.TransformPosition(FVector(1000,-4000,0))},4,Column,B});
            }
        }
        return;
    }
    for(int32 I=0;I<R->Paths.Num();++I)
        Cases.Add({FString::Printf(TEXT("ground_path_%d"),I),{R->Paths[I].A,R->Paths[I].B},0});
    for(int32 Column=0;Column<4;++Column)
    {
        TArray<EW::CityWalkFloor> Floors;
        for(auto F:R->CityFloors)if(F.Column==Column)Floors.Add(F);
        for(int32 Index:{0, Floors.Num()/2+4,Floors.Num()-1})
        {
            if(!Floors.IsValidIndex(Index))continue;
            FEWTraversalCase C;C.Name=FString::Printf(TEXT("tower_%d_ring_z_%.0f"),Column,Floors[Index].Transform.GetLocation().Z);
            C.Kind=1;C.Column=Column;C.Floor=Floors[Index].Transform;Cases.Add(C);
        }
    }
    int32 B=0;
    for(const auto& P:R->Parts)
        if(P.Mesh.ToString().StartsWith(TEXT("UrbanBridge_")) || P.Mesh.ToString().StartsWith(TEXT("UrbanPromenade_")))
        {
            const FVector D=P.Transform.GetRotation().GetAxisX();
            Cases.Add({FString::Printf(TEXT("air_bridge_%d_%s"),B++,*P.Mesh.ToString()),
                {P.Transform.TransformPosition(FVector(-3200,0,0))-D*250,P.Transform.TransformPosition(FVector(3200,0,0))+D*250},0});
        }
    for(int32 Column=0;Column<4;++Column)Cases.Add({FString::Printf(TEXT("lift_%d_up_down"),Column),{},2,Column});
}
void AEWTraversalAudit::SetupAt(FVector P)
{
    auto* C=Player();C->SetTestMovement(FVector::ZeroVector,false);C->ClearHeldInput();C->SetBase(static_cast<UPrimitiveComponent*>(nullptr));
    C->SetActorLocation(Manager()->ToRender({0,0},P)+FVector(0,0,91),false,nullptr,ETeleportType::TeleportPhysics);
    C->ResetSafeLocation();C->SetStreamingHold(true);C->GetCharacterMovement()->MaxWalkSpeed=650;
    LastPosition=C->GetActorLocation();bMeasure=false;BestDistance=DBL_MAX;
    ProgressTime=GetWorld()->GetTimeSeconds();StageStart=FPlatformTime::Seconds();
}
void AEWTraversalAudit::BeginCase()
{
    if(CaseIndex>=Cases.Num())
    {
        if(bVertical){GetGameInstance<UEWGameInstance>()->SaveNow();Finish();return;}
        bMeasure=false;Player()->SetTestMovement(FVector::ZeroVector,false);
        GetGameInstance<UEWGameInstance>()->Visit(HighBookmark);Phase=90;StageStart=FPlatformTime::Seconds();
        Event(TEXT("saved_floor_travel"),HighBookmark.LocalPosition.ToString());return;
    }
    auto& C=Cases[CaseIndex];PointIndex=1;LiftLeg=0;bBelowDeparture=false;LowestDropZ=DBL_MAX;
    if(C.Kind==1)SetupAt(C.Floor.TransformPosition(FVector(2150,-2150,0)));
    else if(C.Kind==2)
    {
        auto* L=Lift(C.Column);if(!L){Finish(TEXT("missing lift actor"));return;}
        int32 Ground=0;
        for(int32 I=1;I<L->Spec.Stops.Num();++I)
            if(FMath::Abs(L->Spec.Stops[I].Height-Manager()->RecipeAt({0,0})->Hub.Z)<FMath::Abs(L->Spec.Stops[Ground].Height-Manager()->RecipeAt({0,0})->Hub.Z))Ground=I;
        SetupAt(L->Spec.Stops[Ground].Landing);PointIndex=Ground;
    }
    else SetupAt(C.Points[0]);
    Event(TEXT("case_setup_teleport"),C.Name);Phase=1;
}
bool AEWTraversalAudit::WalkTo(FVector Local)
{
    auto* P=Player();const FVector Target=Manager()->ToRender({0,0},Local);
    const double D=FVector::Dist2D(Target,P->GetActorLocation());
    if(D<35)
    {
        P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();
        if(FMath::Abs(P->GetActorLocation().Z-Local.Z-88)>45){Finish(TEXT("walk reached XY at wrong floor"));return false;}
        BestDistance=DBL_MAX;ProgressTime=GetWorld()->GetTimeSeconds();return true;
    }
    if(D<BestDistance-10){BestDistance=D;ProgressTime=GetWorld()->GetTimeSeconds();}
    if(GetWorld()->GetTimeSeconds()-ProgressTime>8)
    {
        FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(EWAuditBlocked),false,P);
        const bool Blocked=GetWorld()->SweepSingleByChannel(Hit,P->GetActorLocation(),P->GetActorLocation()+(Target-P->GetActorLocation()).GetSafeNormal2D()*150,
            FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(38,87),Q);
        Finish(FString::Printf(TEXT("stalled %s target=%s distance=%.1f block=%d hit=%s %s normal=%s collider=%s centre=%s extent=%s"),*Cases[CaseIndex].Name,*Target.ToString(),D,Blocked,
            *GetNameSafe(Hit.GetActor()),*GetNameSafe(Hit.Component.Get()),*Hit.ImpactNormal.ToString(),
            Hit.Component.IsValid() && Hit.Component->ComponentTags.Num()>1?*Hit.Component->ComponentTags[1].ToString():TEXT("none"),
            Hit.Component.IsValid()?*Hit.Component->Bounds.Origin.ToString():TEXT("none"),
            Hit.Component.IsValid()?*Hit.Component->Bounds.BoxExtent.ToString():TEXT("none")));return false;
    }
    P->GetCharacterMovement()->MaxWalkSpeed=650;
    P->SetTestMovement(Target-P->GetActorLocation(),true);bMeasure=true;return false;
}
void AEWTraversalAudit::PassCase()
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("case"),Cases[CaseIndex].Name);O->SetBoolField(TEXT("success"),true);
    O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-StageStart);Results.Add(MakeShared<FJsonValueObject>(O));
    if(Cases[CaseIndex].Kind==3 || Cases[CaseIndex].Kind==4)
        O->SetNumberField(TEXT("descent_metres"),(Cases[CaseIndex].Points[0].Z+88-LowestDropZ)/100.);
    Event(TEXT("case_pass"),Cases[CaseIndex].Name);++CaseIndex;BeginCase();
}
void AEWTraversalAudit::Finish(const FString& Failure)
{
    if(bFinished)return;bFinished=true;if(auto* P=Player())P->SetTestMovement(FVector::ZeroVector,false);
    Event(Failure.IsEmpty()?TEXT("pass"):TEXT("failure"),Failure);
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("success"),Failure.IsEmpty());R->SetStringField(TEXT("failure"),Failure);
    R->SetArrayField(TEXT("cases"),Results);R->SetNumberField(TEXT("walked_metres"),Walked/100.);R->SetNumberField(TEXT("ridden_metres"),Ridden/100.);
    R->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);R->SetBoolField(TEXT("resume_only"),bResume);
    R->SetNumberField(TEXT("final_z"),Player()?Player()->GetActorLocation().Z:0);
    R->SetNumberField(TEXT("fall_recoveries"),Player()?Player()->FallRecoveries-InitialFalls:0);
    R->SetNumberField(TEXT("expected_void_recoveries"),ExpectedRecoveries);
    R->SetBoolField(TEXT("vertical_access_audit"),bVertical);
    R->SetBoolField(TEXT("live_input_blocked"),GetGameInstance<UEWGameInstance>()->bScriptedWorldAudit);
    R->SetNumberField(TEXT("hardware_forward_events"),Player()?Player()->ForwardEvents:0);
    R->SetNumberField(TEXT("hardware_look_events"),Player()?Player()->LookEvents:0);
    if(auto* M=Manager()){R->SetNumberField(TEXT("active_colliders"),M->ColliderCount());R->SetNumberField(TEXT("resident_chunks"),M->ResidentCount());}
    R->SetStringField(TEXT("scope"),TEXT("Actual CharacterMovement and moving lifts. Setup teleports excluded from travel distances. Physical controller and rendered GPU acceptance are separate."));
    FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));FFileHelper::SaveStringToFile(S,*ReportPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExit(false);
}
void AEWTraversalAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(bFinished)return;auto* P=Player();auto* M=Manager();if(!P || !M)return;
    const double Now=FPlatformTime::Seconds();
    if(Now-StageStart>(Phase<=1?600:180)){Finish(TEXT("stage timeout"));return;}
    if(Phase==0)
    {
        if(M->IsTravelling() || P->IsStreamingHeld() || !P->GetCharacterMovement()->IsMovingOnGround())return;
        InitialFalls=P->FallRecoveries;
        if(bResume && !bResumeRequested)
        {
            bResumeRequested=true;StageStart=Now;
            GetGameInstance<UEWGameInstance>()->ContinueWorld();
            Event(TEXT("resume_requested"),TEXT("Initial preview finished; now load the saved position."));return;
        }
        if(bResume){Finish(FMath::Abs(P->GetActorLocation().Z-ExpectedZ)<15?FString():TEXT("saved upper floor did not resume at expected height"));return;}
        BuildCases();if(Cases.IsEmpty()){Finish(TEXT("no traversal cases"));return;}BeginCase();return;
    }
    if(P->ForwardEvents || P->StrafeEvents || P->LookEvents || P->JumpEvents)
    {Finish(TEXT("live input contaminated the scripted traversal"));return;}
    const bool VoidCase=Cases.IsValidIndex(CaseIndex) && Cases[CaseIndex].Kind==4;
    if(P->FallRecoveries!=InitialFalls+ExpectedRecoveries && !VoidCase){Finish(TEXT("unexpected fall recovery"));return;}
    if(bMeasure)
    {
        const FVector Change=P->GetActorLocation()-LastPosition;
        if(P->IsLiftRiding())Ridden+=FMath::Abs(Change.Z);else Walked+=Change.Size2D();
    }
    LastPosition=P->GetActorLocation();
    if(Phase==90)
    {
        if(M->IsTravelling() || P->IsStreamingHeld() || !P->GetCharacterMovement()->IsMovingOnGround())return;
        if(FMath::Abs(P->GetActorLocation().Z-HighBookmark.LocalPosition.Z)>20){Finish(TEXT("upper floor revisit changed height"));return;}
        if(!GetGameInstance<UEWGameInstance>()->SaveNow()){Finish(TEXT("upper floor save failed"));return;}
        Finish();return;
    }
    auto& C=Cases[CaseIndex];
    if(Phase==1)
    {
        if(Now-StageStart<.25 || P->IsStreamingHeld() || !M->ReadyAt(M->PlayerCoord()))return;
        if(!P->GetCharacterMovement()->IsMovingOnGround())return;
        if(C.Kind==3 || C.Kind==4)
        {
            P->GetCharacterMovement()->MaxWalkSpeed=350;
            P->Jump();P->SetTestMovement(C.Points[1]-C.Points[0],true);
            Phase=30;bMeasure=true;StageStart=Now;return;
        }
        if(C.Kind==5)
        {
            auto* L=Lift(C.Column);
            if(!L || !L->Call(C.Stop)){Finish(TEXT("landing lift call failed"));return;}
            Phase=40;StageStart=Now;return;
        }
        if(C.Kind==1)
        {
            bool Found=false;FCollisionQueryParams Q(SCENE_QUERY_STAT(EWAuditRing),false,P);
            for(double Radius:{1950.,2150.,2250.})
            {
                TArray<FVector> V;
                for(FVector XY:{FVector(1,-1,0),FVector(1,1,0),FVector(-1,1,0),FVector(-1,-1,0),FVector(1,-1,0)})V.Add(C.Floor.TransformPosition(XY*Radius));
                bool Clear=true;
                for(int32 I=1;I<V.Num();++I)
                    if(GetWorld()->SweepTestByChannel(M->ToRender({0,0},V[I-1])+FVector(0,0,92),M->ToRender({0,0},V[I])+FVector(0,0,92),
                        FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(38,87),Q)){Clear=false;break;}
                if(Clear){C.Points=MoveTemp(V);Found=true;break;}
            }
            if(!Found){Finish(C.Name+TEXT(" no clear capsule circuit"));return;}
            SetupAt(C.Points[0]);Event(TEXT("ring_circuit_setup"),C.Name);Phase=2;return;
        }
        if(C.Kind==2)
        {
            auto* L=Lift(C.Column);if(!L || !L->Call(PointIndex)){Finish(TEXT("ground lift call failed"));return;}
            Phase=20;return;
        }
        Phase=2;
    }
    if(Phase==30)
    {
        if(Now-StageStart>.2)P->StopJumping();
        LowestDropZ=FMath::Min(LowestDropZ,P->GetActorLocation().Z);
        const double Feet=P->GetActorLocation().Z-88;
        if(Feet<C.Points[0].Z-100)bBelowDeparture=true;
        if(C.Kind==4 && P->FallRecoveries==InitialFalls+ExpectedRecoveries+1)
        {
            ++ExpectedRecoveries;P->SetTestMovement(FVector::ZeroVector,false);
            Phase=31;return;
        }
        if(C.Kind==3 && bBelowDeparture)
        {
            P->SetTestMovement(Manager()->ToRender({0,0},C.Points[2])-P->GetActorLocation(),true);
            if(P->GetCharacterMovement()->IsMovingOnGround())
            {
                // A library sill or guard can be the first contact. Continue
                // the real steering input until standing on the public floor.
                const auto& Support=P->GetCharacterMovement()->CurrentFloor.HitResult.Component;
                if(FMath::Abs(Feet-C.Points[2].Z)>200 && Support.IsValid() && Support->ComponentHasTag(TEXT("EWFloor")))
                {Finish(TEXT("drop landed on wrong lower floor"));return;}
                if(FMath::Abs(Feet-C.Points[2].Z)<30 && Support.IsValid() && Support->ComponentHasTag(TEXT("EWFloor")))
                {P->SetTestMovement(FVector::ZeroVector,false);PassCase();return;}
            }
        }
        if(Now-StageStart>15){Finish(TEXT("drop did not reach a lower gallery or void recovery"));return;}
        return;
    }
    if(Phase==31)
    {
        if(P->GetCharacterMovement()->IsMovingOnGround())
        {
            if(FMath::Abs(P->GetActorLocation().Z-88-C.Points[0].Z)>40){Finish(TEXT("void recovery returned to wrong floor"));return;}
            PassCase();
        }
        return;
    }
    if(Phase==40)
    {
        auto* L=Lift(C.Column);if(!L || L->IsMoving())return;
        Phase=41;PointIndex=1;BestDistance=DBL_MAX;ProgressTime=GetWorld()->GetTimeSeconds();
    }
    if(Phase==41)
    {
        if(WalkTo(C.Points[1])){Phase=42;BestDistance=DBL_MAX;ProgressTime=GetWorld()->GetTimeSeconds();}return;
    }
    if(Phase==42)
    {
        auto* L=Lift(C.Column);if(!L){Finish(TEXT("missing lift"));return;}
        if(WalkTo(Manager()->ToLocal({0,0},L->CarPosition())))
        {
            int32 Stop;bool OnCar;
            if(!L->Nearby(P,Stop,OnCar) || !OnCar){Finish(TEXT("aligned door did not permit boarding"));return;}
            Phase=43;BestDistance=DBL_MAX;ProgressTime=GetWorld()->GetTimeSeconds();
        }return;
    }
    if(Phase==43)
    {
        if(WalkTo(C.Points[0]))PassCase();return;
    }
    if(Phase==2)
    {
        if(P->IsStreamingHeld()){ProgressTime=GetWorld()->GetTimeSeconds();return;}
        if(WalkTo(C.Points[PointIndex]) && ++PointIndex>=C.Points.Num())PassCase();return;
    }
    auto* L=Lift(C.Column);if(!L){Finish(TEXT("lift disappeared"));return;}
    if(Phase==20)
    {
        if(L->IsMoving() || P->IsStreamingHeld())return;
        Phase=21;BestDistance=DBL_MAX;ProgressTime=GetWorld()->GetTimeSeconds();
    }
    if(Phase==21)
    {
        if(!WalkTo(M->ToLocal({0,0},L->CarPosition())))return;
        auto* GI=GetGameInstance<UEWGameInstance>();GI->Interact();
        if(GI->Menu()!=EEWMenu::Lift){Finish(TEXT("boarding did not offer lift destinations"));return;}
        const int32 Destination=LiftLeg==0?L->Spec.Stops.Num()-1:0;
        GI->SelectLiftFloor(Destination);
        if(!P->IsLiftRiding() || !L->IsMoving()){Finish(TEXT("selected lift ride did not start"));return;}
        Phase=22;bMeasure=true;StageStart=Now;return;
    }
    if(Phase==22)
    {
        if(L->IsMoving() || P->IsStreamingHeld())return;
        if(P->IsLiftRiding()){Finish(TEXT("rider still locked after arrival"));return;}
        Phase=23;BestDistance=DBL_MAX;ProgressTime=GetWorld()->GetTimeSeconds();
    }
    if(Phase==23)
    {
        if(!WalkTo(L->Spec.Stops[L->FloorIndex()].Landing))return;
        if(LiftLeg==0)
        {
            HighBookmark=M->CurrentPosition();HighBookmark.Name=TEXT("高層階の再開検証");
            if(!GetGameInstance<UEWGameInstance>()->SaveNow()){Finish(TEXT("save on upper landing failed"));return;}
            EW::PlaceBookmark Parsed;FString Error;
            if(!EW::PlaceBookmark::Parse(HighBookmark.Code(),Parsed,Error) || FMath::Abs(Parsed.LocalPosition.Z-HighBookmark.LocalPosition.Z)>1)
            {Finish(TEXT("upper bookmark round trip failed"));return;}
            Event(TEXT("upper_floor_saved"),HighBookmark.LocalPosition.ToString());LiftLeg=1;Phase=21;BestDistance=DBL_MAX;ProgressTime=GetWorld()->GetTimeSeconds();return;
        }
        PassCase();
    }
}
