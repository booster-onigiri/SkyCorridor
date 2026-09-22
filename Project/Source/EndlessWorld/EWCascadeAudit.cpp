#include "EWCascadeAudit.h"
#include "EWOuterWater.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWExplorationPlan.h"
#include "EWDayCycle.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

AEWCascadeAudit::AEWCascadeAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWCascadeAudit::BeginPlay()
{
    Super::BeginPlay();Started=Stage=Progress=FPlatformTime::Seconds();Resume=FParse::Param(FCommandLine::Get(),TEXT("EWCascade86Resume"));
    if(Resume)Phase=-1;
    if(!FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report) || IFileManager::Get().FileExists(*Report)){Done=true;FPlatformMisc::RequestExit(false);}
}
bool AEWCascadeAudit::Check(bool Pass,const FString& Name)
{
    auto C=MakeShared<FJsonObject>();C->SetBoolField(TEXT("pass"),Pass);C->SetStringField(TEXT("check"),Name);Checks.Add(MakeShared<FJsonValueObject>(C));
    if(!Pass)Finish(Name);return Pass;
}
void AEWCascadeAudit::Finish(const FString& Error)
{
    if(Done)return;Done=true;auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(P)P->SetTestMovement(FVector::ZeroVector,false);
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);O->SetArrayField(TEXT("checks"),Checks);
    O->SetArrayField(TEXT("captures"),Captures);O->SetNumberField(TEXT("walked_metres"),Walked/100.);O->SetNumberField(TEXT("fall_recoveries"),P?P->FallRecoveries-Falls:0);
    O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);O->SetBoolField(TEXT("resume"),Resume);
    O->SetStringField(TEXT("scope"),TEXT("16 district recipes and 64 portal contracts; physical old-city boundary crossing, four districts and eight discoveries, upper ramps, water recovery, four lighting phases, independent save restart; silent"));
    FString T;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&T));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);
    FFileHelper::SaveStringToFile(T,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);FPlatformMisc::RequestExit(false);
}
void AEWCascadeAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>1200 || (Age>180 && Phase!=2)){Finish(FString::Printf(TEXT("timeout phase %d point %d"),Phase,Point));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !M || !P || !PC || M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt(M->PlayerCoord()))return;
    auto* Move=P->GetCharacterMovement();const auto W=EW::WorldDescriptor::ReferenceWorld();
    auto Next=[&](int32 N){Phase=N;Stage=Progress=Now;Best=DBL_MAX;LastCoord=M->PlayerCoord();Last=M->ToLocal(LastCoord,P->GetActorLocation());
        const FString S=FString::Printf(TEXT("{\"phase\":%d,\"point\":%d,\"photo\":%d,\"seconds\":%.1f}"),Phase,Point,Photo,Now-Started);
        FFileHelper::SaveStringToFile(S,*(Report+TEXT(".progress.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);};
    auto Walk=[&](const Step& S)->bool
    {
        const auto Pos=P->GetActorLocation();Walked+=FVector::Dist2D(Pos,M->ToRender(LastCoord,Last));LastCoord=M->PlayerCoord();Last=M->ToLocal(LastCoord,Pos);
        const auto Goal=M->ToRender(S.Coord,S.Position)+FVector(0,0,90);const auto D=Goal-Pos;const double Distance=D.Size2D();
        if(Distance<22){P->SetTestMovement(FVector::ZeroVector,false);return true;}
        if(Distance<Best-5){Best=Distance;Progress=Now;}else if(Now-Progress>8){Finish(FString::Printf(TEXT("walk blocked at point %d in %s, position %s goal %s"),Point,*S.Coord.Text(),*M->ToLocal(S.Coord,Pos).ToString(),*S.Position.ToString()));return false;}
        PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));P->SetTestMovement(D.GetSafeNormal2D(),true);return false;
    };
    if(Phase==-1 && Move->IsMovingOnGround()){G->ContinueWorld();Next(0);return;}
    if(Phase==0 && Move->IsMovingOnGround())
    {
        Falls=P->FallRecoveries;InitialRecords=G->RecordCount();FString Error;TSet<FString> Meshes;
        for(int64 X=3;X<=6;++X)for(int64 Y=-1;Y<=2;++Y)
        {
            const EW::ChunkCoord C{X,Y};const auto R=EW::GenerateChunk(W,C);
            if(!Check(R.bCascadeCity && !R.bFallback && R.Validate(Error),TEXT("valid water district ")+C.Text()+TEXT(" ")+Error))return;
            for(int I=0;I<4;++I)
            {
                auto Other=C;if(I<2)Other.X+=I==0?-1:1;else Other.Y+=I==2?-1:1;
                const auto N=EW::GenerateChunk(W,Other);const auto A=R.Portals[I],B=N.Portals[I^1];
                const FVector Offset((Other.X-C.X)*EW::ChunkSize,(Other.Y-C.Y)*EW::ChunkSize,0);
                if(!Check(A.Position.Equals(B.Position+Offset,.01) && A.Width==B.Width,TEXT("matching portal ")+C.Text()+FString::Printf(TEXT("/%d"),I)))return;
            }
            for(const auto& B:R.Places){EW::PlaceBookmark Parsed;if(!Check(EW::PlaceBookmark::Parse(B.Code(),Parsed,Error) && Parsed.Id==B.Id && Parsed.Kind==B.Kind,TEXT("unique bookmark roundtrip ")+B.Id))return;}
            for(const auto& Part:R.Parts)Meshes.Add(Part.Mesh.ToString());
            if(X<6 && !Check(FMath::Abs(EWOuterWater::WaterHeight(W,C)-EWOuterWater::WaterHeight(W,{X+1,Y})-450)<.01,TEXT("connected eastward cascade ")+C.Text()))return;
        }
        for(const auto& Name:Meshes)if(!Check(LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/EndlessWorld/Kit/SM_")+Name))!=nullptr,TEXT("asset resolves ")+Name))return;
        const auto City=EW::GenerateChunk(W,{0,0});
        if(!Check(!City.bCascadeCity && City.Places.Num()==7 && EWExplorationPlan::Find(City,5)!=nullptr,TEXT("original city and all six public places retained")))return;
        if(!Check(!EWOuterWater::Contains(W,{2,0}) && !EWOuterWater::Contains(W,{7,0}) && !EWOuterWater::Contains(W,{3,3}),TEXT("exterior expansion bounded to 16 districts")))return;
        const EW::ChunkCoord Visited[]={{3,0},{4,0},{4,1},{5,1}};
        if(Resume)
        {
            if(!Check(M->PlayerCoord()==EW::ChunkCoord{5,1} && FVector::Dist2D(M->ToLocal({5,1},P->GetActorLocation()),FVector(6400,10800,0))<70,TEXT("resume returns to last belvedere")))return;
            for(auto C:Visited)for(const auto& B:EW::GenerateChunk(W,C).Places)if(!Check(G->IsRecorded(B),TEXT("saved discovery restored ")+B.Id))return;
            if(!Check(G->SaveNow(),TEXT("extracted launcher can save restored life data")))return;Finish();return;
        }
        // Begin on the real final old-city connector, then walk across the edge.
        const auto Old=EW::GenerateChunk(W,{2,0});const auto Portal=Old.Portals[1].Position;FVector Approach=Portal;
        for(const auto& Path:Old.Paths)
        {if(Path.A.Equals(Portal,.01)){Approach=FMath::Lerp(Path.A,Path.B,FMath::Min(.4,700./FVector::Dist2D(Path.A,Path.B)));break;}
         if(Path.B.Equals(Portal,.01)){Approach=FMath::Lerp(Path.B,Path.A,FMath::Min(.4,700./FVector::Dist2D(Path.A,Path.B)));break;}}
        Route.Add({{2,0},Approach});Route.Add({{2,0},Portal});
        for(int32 I=0;I<4;++I)
        {
            const auto C=Visited[I];const auto R=EW::GenerateChunk(W,C);const int32 In=I==2?2:0;const int32 Out=I==1?3:1;
            auto Add=[&](FVector V,int32 K=0){Route.Add({C,V,K});};
            const int32 B=In*4;for(int32 J=0;J<4;++J)Add(R.Paths[B+J].B);
            Add(R.Places[0].LocalPosition-FVector(0,0,100),15);Add(R.Hub);
            Add(R.Paths[2].B);Add(R.Paths[2].A); // reach the actual west ramp junction
            const auto U=EWOuterWater::UpperRoute(R);
            for(int32 J=1;J<U.Num();++J)Add(U[J],J==3?16:0);
            if(Out==3){Add(R.Paths[6].B);Add(R.Hub);Add(R.Paths[15].A);Add(R.Paths[14].A);Add(R.Paths[13].A);Add(R.Paths[12].A);}
            else if(I<3){Add(R.Paths[5].A);Add(R.Paths[4].A);}
        }
        EW::PlaceBookmark Start;Start.WorldCode=W.Code();Start.Coord={2,0};Start.LocalPosition=Approach+FVector(0,0,100);Start.Yaw=0;G->Visit(Start);Next(1);return;
    }
    if(Phase==1 && Age>2 && Move->IsMovingOnGround())
    {PC->SetViewTarget(P);G->SetMenu(EEWMenu::None);Move->MaxWalkSpeed=480;Point=0;Next(2);return;}
    if(Phase==2)
    {
        if(Walk(Route[Point]))
        {
            const auto& S=Route[Point];const double Z=M->ToRender(S.Coord,S.Position).Z+90;
            if(!Check(Move->IsMovingOnGround() && FMath::Abs(P->GetActorLocation().Z-Z)<24 && P->FallRecoveries==Falls,FString::Printf(TEXT("physical walk point %d in %s"),Point,*S.Coord.Text())))return;
            if(S.Kind){const auto R=M->RecipeAt(S.Coord);G->RecordNearest();if(!Check(R && G->IsRecorded(R->Places[S.Kind-15]),TEXT("walk-in discovery ")+S.Coord.Text()+FString::FromInt(S.Kind)))return;}
            ++Point;Progress=Now;Best=DBL_MAX;
            if(Point==Route.Num()){if(!Check(G->RecordCount()==InitialRecords+8,TEXT("eight distinct discoveries, existing records retained")))return;Next(20);}
        }
        return;
    }
    if(Phase==20 && Age>1 && Move->IsMovingOnGround())
    {
        Recovery=M->ToLocal({5,1},P->GetActorLocation());P->SetTestMovement(FVector::ZeroVector,false);
        P->SetActorLocation(M->ToRender({5,1},FVector(9000,6500,EWOuterWater::WaterHeight(W,{5,1})+250)),false);Move->StopMovementImmediately();Move->SetMovementMode(MOVE_Falling);Next(21);return;
    }
    if(Phase==21 && P->FallRecoveries>Falls && Move->IsMovingOnGround())
    {
        if(!Check(P->FallRecoveries==Falls+1 && FVector::Dist2D(M->ToLocal({5,1},P->GetActorLocation()),Recovery)<180,TEXT("intentional fall into water returns to dry safe path")))return;
        Photo=0;Next(30);return;
    }
    if(Phase==30)
    {
        if(!Camera)Camera=GetWorld()->SpawnActor<ACameraActor>();const EW::ChunkCoord C{5,1};const double H=EWOuterWater::Ground(W,C);const int32 V=Photo/4;
        const FVector Eyes[]={FVector(1000,6100,H+170),FVector(6400,10800,H+1065),FVector(6100,7000,H+170),FVector(1050,4400,H+2200),FVector(-16000,18000,H+13000)};
        const FVector Aims[]={FVector(11600,8000,H+3100),FVector(15000,3000,H+3800),FVector(2300,2300,H+3600),FVector(2300,3480,H+2300),FVector(6400,1000,H+2800)};
        const auto Eye=M->ToRender(C,Eyes[V]);Camera->SetActorLocationAndRotation(Eye,(M->ToRender(C,Aims[V])-Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(85);PC->SetViewTarget(Camera);
        const double Hours[]={7,12,18,23};G->DayCycle->SetAuditHour(Hours[Photo%4]);Next(31);return;
    }
    if(Phase==31 && Age>4)
    {
        auto C=MakeShared<FJsonObject>();C->SetNumberField(TEXT("view"),Photo/4);C->SetNumberField(TEXT("hour_index"),Photo%4);C->SetObjectField(TEXT("lighting"),G->DayCycle->Evidence());Captures.Add(MakeShared<FJsonValueObject>(C));
        FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/FString::Printf(TEXT("cascade-view-%d-hour-%d.png"),Photo/4,Photo%4),false,false);Next(32);return;
    }
    if(Phase==32 && Age>1)
    {
        if(++Photo<20){Next(30);return;}
        PC->SetViewTarget(P);G->DayCycle->SetAuditHour(12);const auto R=EW::GenerateChunk(W,{5,1});G->Visit(R.Places[1]);Next(40);return;
    }
    if(Phase==40 && Age>2 && Move->IsMovingOnGround())
    {if(!Check(G->SaveNow(),TEXT("normal save persists outer position and all discoveries")))return;Finish();}
}
