#include "EWExploreAudit.h"
#include "EWExplorationPlan.h"
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
#include "UnrealClient.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Rendering/NaniteResources.h"
#include "DynamicRHI.h"
#include "HAL/IConsoleManager.h"
#include "Components/StaticMeshComponent.h"
#include "PrimitiveSceneProxy.h"

AEWExploreAudit::AEWExploreAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWExploreAudit::BeginPlay()
{
    Super::BeginPlay();Started=Stage=Progress=FPlatformTime::Seconds();Resume=FParse::Param(FCommandLine::Get(),TEXT("EWExplore85Resume"));
    if(Resume)Phase=-1;
    if(!FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report) || IFileManager::Get().FileExists(*Report)){Done=true;FPlatformMisc::RequestExit(false);}
    if(FParse::Param(FCommandLine::Get(),TEXT("EWQuality93Visual")))
    {
        // Isolated packaged-render diagnostics, never a physical route pass.
        for(const auto& Setting:TArray<TPair<const TCHAR*,const TCHAR*>>{
            {TEXT("EWQualityNanite="),TEXT("r.Nanite")},
            {TEXT("EWQualityProxy="),TEXT("r.Nanite.ProxyRenderMode")},
            {TEXT("EWQualityPool="),TEXT("r.Nanite.Streaming.StreamingPoolSize")}})
        {
            int32 Value=0;
            if(FParse::Value(FCommandLine::Get(),Setting.Key,Value))
                if(auto* V=IConsoleManager::Get().FindConsoleVariable(Setting.Value))V->Set(Value,ECVF_SetByConsole);
        }
    }
}
bool AEWExploreAudit::Check(bool Pass,const FString& Name)
{
    auto C=MakeShared<FJsonObject>();C->SetBoolField(TEXT("pass"),Pass);C->SetStringField(TEXT("check"),Name);Checks.Add(MakeShared<FJsonValueObject>(C));
    if(!Pass)Finish(Name);return Pass;
}
void AEWExploreAudit::Finish(const FString& Error)
{
    if(Done)return;Done=true;auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(P)P->SetTestMovement(FVector::ZeroVector,false);
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);O->SetArrayField(TEXT("checks"),Checks);
    O->SetArrayField(TEXT("captures"),Captures);O->SetNumberField(TEXT("walked_metres"),Walked/100.);O->SetNumberField(TEXT("fall_recoveries"),P?P->FallRecoveries-Falls:0);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    O->SetStringField(TEXT("scope"),TEXT("isolated actual public halls walking, gallery stairs, seats, three lift rides, six discoveries, four lighting phases; silent"));
    const bool VisualOnly=FParse::Param(FCommandLine::Get(),TEXT("EWQuality93Visual"));
    O->SetBoolField(TEXT("visual_only"),VisualOnly);
    if(VisualOnly)O->SetStringField(TEXT("scope"),TEXT("isolated cooked rendering comparison only; no walking or interaction acceptance"));
    // Preserve the actual cooked rendering state alongside the physical audit.
    // A successful route cannot detect missing Nanite streaming data.
    auto Rendering=MakeShared<FJsonObject>();
    Rendering->SetStringField(TEXT("rhi"),GDynamicRHI?GDynamicRHI->GetName():TEXT("none"));
    Rendering->SetNumberField(TEXT("feature_level"),int32(GetWorld()->GetFeatureLevel()));
    for(const TCHAR* Key:{TEXT("r.Nanite"),TEXT("r.Nanite.ProxyRenderMode"),TEXT("r.Nanite.Streaming.StreamingPoolSize")})
        if(auto* V=IConsoleManager::Get().FindConsoleVariable(Key))Rendering->SetNumberField(Key,V->GetInt());
    TArray<TSharedPtr<FJsonValue>> RenderMeshes;
    for(int32 I=0;I<6;++I)
    {
        auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("place"),I);
        const FString Path=FString::Printf(TEXT("/Game/EndlessWorld/Kit/SM_Explore85Furniture%d.SM_Explore85Furniture%d"),I,I);
        if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path))if(auto* Data=Mesh->GetRenderData())
        {
            Row->SetBoolField(TEXT("valid_nanite_data"),Data->HasValidNaniteData());
            if(Data->NaniteResourcesPtr)
            {
                Row->SetNumberField(TEXT("nanite_input_triangles"),Data->NaniteResourcesPtr->NumInputTriangles);
                Row->SetNumberField(TEXT("root_bytes"),Data->NaniteResourcesPtr->RootData.Num());
                Row->SetNumberField(TEXT("streaming_bytes"),double(Data->NaniteResourcesPtr->StreamablePages.GetBulkDataSize()));
                Row->SetNumberField(TEXT("pages"),Data->NaniteResourcesPtr->PageStreamingStates.Num());
            }
            if(Data->LODResources.Num())Row->SetNumberField(TEXT("fallback_triangles"),Data->LODResources[0].GetNumTriangles());
        }
        RenderMeshes.Add(MakeShared<FJsonValueObject>(Row));
    }
    Rendering->SetArrayField(TEXT("public_meshes"),RenderMeshes);O->SetObjectField(TEXT("rendering"),Rendering);
    TArray<TSharedPtr<FJsonValue>> Proxies;
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TArray<UStaticMeshComponent*> Components;It->GetComponents(Components);
        for(auto* C:Components)if(C->GetStaticMesh() && C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_Explore85Furniture")))
        {
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("mesh"),C->GetStaticMesh()->GetName());
            Row->SetBoolField(TEXT("nanite_proxy"),C->GetSceneProxy() && C->GetSceneProxy()->IsNaniteMesh());
            Row->SetBoolField(TEXT("disallow_nanite"),C->bDisallowNanite);Proxies.Add(MakeShared<FJsonValueObject>(Row));
        }
    }
    Rendering->SetArrayField(TEXT("scene_proxies"),Proxies);
    FString T;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&T));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);FFileHelper::SaveStringToFile(T,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);FPlatformMisc::RequestExit(false);
}
void AEWExploreAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>1000 || Age>160){Finish(FString::Printf(TEXT("timeout phase %d place %d point %d"),Phase,Index,Point));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !M || !P || !PC || M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt({0,0}))return;
    auto* Move=P->GetCharacterMovement();const auto R=M->RecipeAt({0,0});
    auto Next=[&](int32 N){Phase=N;Stage=Progress=Now;Best=DBL_MAX;Last=P->GetActorLocation();
        const FString S=FString::Printf(TEXT("{\"phase\":%d,\"place\":%d,\"point\":%d,\"seconds\":%.1f}"),Phase,Index,Point,Now-Started);
        FFileHelper::SaveStringToFile(S,*(Report+TEXT(".progress.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);};
    auto Walk=[&](FVector Goal)->bool
    {
        const auto Pos=P->GetActorLocation();Walked+=FVector::Dist2D(Pos,Last);Last=Pos;const auto D=Goal-Pos;const double Distance=D.Size2D();
        if(Distance<22){P->SetTestMovement(FVector::ZeroVector,false);return true;}
        if(Distance<Best-5){Best=Distance;Progress=Now;}else if(Now-Progress>7){Finish(FString::Printf(TEXT("walk blocked phase %d place %d point %d at %s goal %s"),Phase,Index,Point,*Pos.ToString(),*Goal.ToString()));return false;}
        PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));P->SetTestMovement(D.GetSafeNormal2D(),true);return false;
    };
    if(Phase==-1 && Move->IsMovingOnGround()){G->ContinueWorld();Next(0);return;}
    if(Phase==0 && Move->IsMovingOnGround())
    {
        Falls=P->FallRecoveries;InitialRecords=G->RecordCount();FString Error;
        if(!Check(R && !R->bFallback && R->Validate(Error),TEXT("city recipe valid without fallback")))return;
        for(int I=0;I<6;++I)
        {
            const auto* Room=EWExplorationPlan::Find(*R,I);if(!Check(Room!=nullptr,FString::Printf(TEXT("public destination %d exists"),I)))return;Rooms.Add(*Room);
            const auto B=EWExplorationPlan::Bookmark(*R,*Room);EW::PlaceBookmark Read;
            if(!Check(EW::PlaceBookmark::Parse(B.Code(),Read,Error) && Read.Id==B.Id && Read.Kind==B.Kind,FString::Printf(TEXT("place %d code roundtrip keeps unique identity"),I)))return;
        }
        if(!Check(R->Places.Num()==7,TEXT("six discoveries preserve the original plaza bookmark")))return;
        if(FParse::Param(FCommandLine::Get(),TEXT("EWQuality93Visual")))
        {
            Index=2;FParse::Value(FCommandLine::Get(),TEXT("EWQualityPlace="),Index);
            if(!Check(Index>=0 && Index<Rooms.Num(),TEXT("visual destination is valid")))return;
            G->VisitPublicPlace(Index);Next(40);return;
        }
        if(Resume)
        {
            const auto Q=Rooms[5].Frame.InverseTransformPosition(M->ToLocal({0,0},P->GetActorLocation()));
            if(!Check(FMath::Abs(Q.X)<70 && FMath::Abs(Q.Y+2100)<70,TEXT("resume restores conservatory entrance")))return;
            for(const auto& Room:Rooms)if(!Check(G->IsRecorded(EWExplorationPlan::Bookmark(*R,Room)),TEXT("public discovery survived restart")))return;
            if(!Check(G->SaveNow(),TEXT("extracted release saves existing data")))return;Finish();return;
        }
        G->VisitPublicPlace(0);Next(1);return;
    }
    if(Phase==40 && Age>2 && Move->IsMovingOnGround())
    {G->SetMenu(EEWMenu::None);Photo=1;Next(3);return;}
    if(Phase==1 && Age>2 && Move->IsMovingOnGround())
    {
        PC->SetViewTarget(P);G->SetMenu(EEWMenu::None);Move->MaxWalkSpeed=420;
        const auto T=Rooms[Index].Frame;
        // Start at the existing lift's fixed landing, then walk the real ring to a door.
        const EW::LiftStop* Stop=nullptr;
        for(const auto& L:R->Lifts)for(const auto& S:L.Stops)if(S.Label.StartsWith(EWExplorationPlan::Name(Index)))Stop=&S;
        if(!Check(Stop!=nullptr,TEXT("named public destination is served by an existing lift")))return;
        auto A=T.InverseTransformPosition(Stop->Entry);A.Z=0;
        P->SetActorLocation(M->ToRender({0,0},Stop->Entry)+FVector(0,0,90),false);Move->StopMovementImmediately();P->ResetSafeLocation();
        Route.Reset();
        if(FMath::Abs(A.Y)>FMath::Abs(A.X))
        {const double Side=A.X>=0?2100.:-2100.;Route.Add(T.TransformPosition(FVector(A.X,FMath::Sign(A.Y)*2100,0)));if(A.Y>0){Route.Add(T.TransformPosition(FVector(Side,2100,0)));Route.Add(T.TransformPosition(FVector(Side,-2100,0)));}}
        else
        {const double Side=FMath::Sign(A.X)*2100.;Route.Add(T.TransformPosition(FVector(Side,A.Y,0)));Route.Add(T.TransformPosition(FVector(Side,-2100,0)));}
        Route.Add(T.TransformPosition(FVector(0,-2100,0)));
        Route.Add(T.TransformPosition(FVector(0,-1450,0)));
        for(double X:{-1700.,1700.})
            for(FVector V:{FVector(X,-1450,0),FVector(X,0,0),FVector(FMath::Sign(X)*2100,0,0),FVector(X,0,0),FVector(X,-1450,0),FVector(0,-1450,0)})Route.Add(T.TransformPosition(V));
        Route.Append(EWInteriors::Route(Rooms[Index]));Point=0;Next(2);return;
    }
    if(Phase==2)
    {
        if(Walk(M->ToRender({0,0},Route[Point])+FVector(0,0,90)))
        {
            const double Z=M->ToRender({0,0},Route[Point]).Z+90;
            if(!Check(Move->IsMovingOnGround() && FMath::Abs(P->GetActorLocation().Z-Z)<16 && P->FallRecoveries==Falls,FString::Printf(TEXT("place %d physical walk point %d"),Index,Point)))return;
            const auto Q=Rooms[Index].Frame.InverseTransformPosition(M->ToLocal({0,0},P->GetActorLocation()));
            if(FVector::Dist2D(Q,FVector(0,500,0))<40 && Q.Z<150)
            {
                const auto B=EWExplorationPlan::Bookmark(*R,Rooms[Index]);G->RecordNearest();
                if(!Check(G->IsRecorded(B),TEXT("discovery recorded at interior marker")))return;
            }
            if(FVector::Dist2D(Q,FVector(600,-1450,0))<40)
            {
                if(!Check(EWExplorationPlan::Sit(G) && P->IsSeated(),TEXT("rest bench accepts seated player")))return;
                if(!Check(P->LeaveSeat(),TEXT("rest bench has a clear standing exit")))return;
            }
            ++Point;Best=DBL_MAX;Progress=Now;if(Point==Route.Num()){Photo=0;Next(3);}return;
        }
        return;
    }
    if(Phase==3)
    {
        if(!Camera)Camera=GetWorld()->SpawnActor<ACameraActor>();const auto T=Rooms[Index].Frame;
        const FVector E=Photo==5?FVector(500,1500,585):Photo==4?FVector(0,-2450,168):FVector(-120,-1420,165);
        const FVector A=Photo==5?FVector(0,-800,150):Photo==4?FVector(0,550,650):FVector(0,550,Index==2?430:270);
        const auto Eye=M->ToRender({0,0},T.TransformPosition(E));
        Camera->SetActorLocationAndRotation(Eye,(M->ToRender({0,0},T.TransformPosition(A))-Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(85);PC->SetViewTarget(Camera);
        const double Hours[]={7,12,18,23,12,12};G->DayCycle->SetAuditHour(Hours[Photo]);Next(4);return;
    }
    if(Phase==4 && Age>(FParse::Param(FCommandLine::Get(),TEXT("EWQuality93Visual"))?12:4))
    {
        auto C=MakeShared<FJsonObject>();C->SetNumberField(TEXT("place"),Index);C->SetNumberField(TEXT("view"),Photo);C->SetObjectField(TEXT("lighting"),G->DayCycle->Evidence());Captures.Add(MakeShared<FJsonValueObject>(C));
        FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/FString::Printf(TEXT("place-%d-view-%d.png"),Index,Photo),false,false);Next(5);return;
    }
    if(Phase==5 && Age>1)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("EWQuality93Visual")))
        {if(Photo==1){Photo=5;Next(3);}else Finish();return;}
        if(++Photo<(Index==2?6:5)){Next(3);return;}
        PC->SetViewTarget(P);G->DayCycle->SetAuditHour(12);
        if(Index%2==0){Next(20);return;}
        if(++Index<6){G->VisitPublicPlace(Index);Next(1);return;}Next(30);return;
    }
    if(Phase==20)
    {
        Lift=nullptr;
        for(TActorIterator<AEWLift> It(GetWorld());It;++It)for(int I=0;I<It->Spec.Stops.Num();++I)
            if(It->Spec.Stops[I].Label.StartsWith(EWExplorationPlan::Name(Index))){Lift=*It;From=I;To=I+1;}
        if(!Check(Lift && To<Lift->Spec.Stops.Num() && Lift->Spec.Stops[To].Label.StartsWith(EWExplorationPlan::Name(Index+1)),TEXT("paired public floors are adjacent lift stops")))return;
        P->SetActorLocation(Lift->GetActorTransform().TransformPosition(Lift->Spec.Stops[From].Entry)+FVector(0,0,92),false);Move->StopMovementImmediately();P->ResetSafeLocation();
        if(!Check(Lift->Call(From),TEXT("call car to public floor")))return;Next(21);return;
    }
    if(Phase==21 && !Lift->IsMoving() && Age>1){Next(22);return;}
    if(Phase==22){if(Walk(Lift->CarPosition()+FVector(0,0,90))){if(!Check(Lift->Ride(P,To),TEXT("board lift from public floor")))return;Next(23);}return;}
    if(Phase==23 && !Lift->IsMoving() && Age>1){Next(24);return;}
    if(Phase==24)
    {
        if(Walk(Lift->GetActorTransform().TransformPosition(Lift->Spec.Stops[To].Entry)+FVector(0,0,90)))
        {if(!Check(Move->IsMovingOnGround() && Lift->FloorIndex()==To && P->FallRecoveries==Falls,TEXT("ride and exit at paired public destination")))return;
            ++Index;G->VisitPublicPlace(Index);Next(1);}return;
    }
    if(Phase==30)
    {
        if(!Check(G->RecordCount()==InitialRecords+6,TEXT("six unique discoveries added without duplicates")))return;
        G->VisitPublicPlace(5);Next(31);return;
    }
    if(Phase==31 && Age>2 && Move->IsMovingOnGround())
    {G->SetMenu(EEWMenu::Explore);Next(32);return;}
    if(Phase==32 && Age>1){FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/TEXT("exploration-menu.png"),true,false);Next(33);return;}
    if(Phase==33 && Age>1){G->SetMenu(EEWMenu::None);if(!Check(G->SaveNow(),TEXT("normal save keeps discoveries and final public entrance")))return;Finish();}
}
