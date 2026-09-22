#include "EWSky92Audit.h"
#include "EWSky92Plan.h"
#include "EWOuterWater.h"
#include "EWGameInstance.h"
#include "EWTerminal.h"
#include "EWPhotoMode.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWLift.h"
#include "EWDayCycle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

AEWSky92Audit::AEWSky92Audit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWSky92Audit::BeginPlay()
{Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);}
void AEWSky92Audit::Next(int32 Value)
{
    Phase=Value;Stage=Progress=FPlatformTime::Seconds();Best=DBL_MAX;
    FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"phase\":%d,\"site\":%d,\"point\":%d,\"app\":%d,\"seconds\":%.2f}"),Phase,Site,Point,App,Stage-Started),*(Report+TEXT(".progress.json")));
}
bool AEWSky92Audit::Check(bool Pass,const FString& Label)
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("pass"),Pass);O->SetStringField(TEXT("check"),Label);
    Checks.Add(MakeShared<FJsonValueObject>(O));if(!Pass)Finish(Label);return Pass;
}
void AEWSky92Audit::Finish(const FString& Error)
{
    if(Done)return;Done=true;auto* G=GetGameInstance<UEWGameInstance>();
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(P)P->SetTestMovement(FVector::ZeroVector,false);
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);
    O->SetArrayField(TEXT("checks"),Checks);O->SetNumberField(TEXT("walked_metres"),Walked/100.);
    O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);O->SetNumberField(TEXT("fall_recoveries"),P?P->FallRecoveries:0);
    if(G && G->Terminal)O->SetObjectField(TEXT("terminal"),G->Terminal->Evidence());
    O->SetStringField(TEXT("scope"),TEXT("Six real projected phone buttons, three new landmarks, full physical walk from each existing water-city access ramp, normal lift boarding/ride/exit and room traversal; silent isolated data. Inter-district train route unchanged."));
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExit(false);
}
void AEWSky92Audit::Prepare(UEWGameInstance* G)
{
    auto* M=G->Manager.Get();const auto C=EWSky92Plan::Coord(Site);const auto W=M->Descriptor();
    const auto R=EW::GenerateChunk(W,C);const auto U=EWOuterWater::UpperRoute(R);const auto F=EWSky92Plan::Frame(W,Site);const double H=R.Hub.Z;
    Route.Reset();Point=0;auto Add=[&](FVector V,int A=0){Route.Add({V,A});};
    Add(U[0]);Add(FVector(U[1].X,10500,H+900));Add(U[1]);
    if(Site==0)
    {
        Add(FVector(4450,10800,H+900));Add(F.TransformPosition(FVector(0,-1600,0)));
        Add(F.TransformPosition(FVector(-600,-1450,0)));
    }
    else if(Site==1)
    {
        Add(FVector(4980,10800,H+900),1);Add(FVector(5300,10800,H+900),2);
        Add(FVector(4980,10800,F.GetLocation().Z));Add(FVector(4400,10800,F.GetLocation().Z));
        for(auto V:{FVector(0,-1450,0),FVector(0,-200,0),FVector(-650,350,0)})Add(F.TransformPosition(V));
    }
    else
    {
        Add(FVector(5000,10800,H+900));Add(FVector(6400,10800,H+900));Add(FVector(6400,11980,H+900),1);Add(FVector(6400,12300,H+900),2);
        Add(FVector(6400,11980,F.GetLocation().Z));Add(FVector(6400,10150,F.GetLocation().Z));
        for(auto V:{FVector(0,-3400,0),FVector(0,-2100,0),FVector(0,0,0),FVector(0,1100,0),FVector(-400,1160,0)})Add(F.TransformPosition(V));
    }
    EW::PlaceBookmark Start;Start.WorldCode=W.Code();Start.Coord=C;Start.LocalPosition=Route[0].Local+FVector(0,0,91);
    Start.Yaw=(Route[1].Local-Route[0].Local).Rotation().Yaw;G->Visit(Start);Next(10);
}
void AEWSky92Audit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>1000 || Age>150){Finish(FString::Printf(TEXT("timeout phase %d site %d point %d"),Phase,Site,Point));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(!G || !P || !G->Manager || !G->Terminal || !G->PhotoMode)return;
    auto* T=G->Terminal.Get();auto* M=G->Manager.Get();auto* Move=P->GetCharacterMovement();auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt(M->PlayerCoord()))return;
    const int Pages[]={5,1,2,3,4,-1};const FVector2D Taps[]={{200,270},{520,270},{200,680},{520,680},{200,1090},{520,1090}};
    auto Capture=[&](FString Label){FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/Label+TEXT(".png"),false,false);};
    if(Phase==0 && Move->IsMovingOnGround() && T->Places().Num()>0)
    {
        if(!Check(T->Places().Num()==15 && T->Evidence()->GetBoolField(TEXT("journal_integrity")),TEXT("15 memories and independent journal ready")))return;
        for(int I=0;I<3;++I)
        {
            const auto R=EW::GenerateChunk(M->Descriptor(),EWSky92Plan::Coord(I));FString Error;
            if(!Check(R.Validate(Error),TEXT("valid landmark district ")+FString::FromInt(I)+Error))return;
            const auto* Marker=R.Places.FindByPredicate([I](const auto& B){return B.Kind==17+I;});EW::PlaceBookmark Parsed;
            if(!Check(Marker && EW::PlaceBookmark::Parse(Marker->Code(),Parsed,Error) && Parsed.Id==Marker->Id,TEXT("new landmark bookmark roundtrip ")+FString::FromInt(I)))return;
            if(!Check(R.Places[0].Kind==15 && R.Places[1].Kind==16,TEXT("existing district marker order retained ")+FString::FromInt(I)))return;
        }
        for(const TCHAR* N:{TEXT("Sky92Phone"),TEXT("Sky92Market"),TEXT("Sky92Cloister"),TEXT("Sky92Castle"),TEXT("Sky92ConePavilion")})
            if(!Check(LoadObject<UStaticMesh>(nullptr,*(FString(TEXT("/Game/EndlessWorld/Kit/SM_"))+N))!=nullptr,FString(TEXT("cooked asset resolves "))+N))return;
        T->Open();Next(1);return;
    }
    if(Phase==1 && Age>3){T->CaptureScreen(FPaths::GetPath(Report)/TEXT("phone-home.png"));if(!Check(T->PreviewTap(Taps[App].X,Taps[App].Y),TEXT("projected touch delivered ")+FString::FromInt(App)))return;Next(2);return;}
    if(Phase==2 && Age>2)
    {
        if(!Check(App==5?G->PhotoMode->Active():T->PageIndex()==Pages[App],TEXT("home icon opens actual app ")+FString::FromInt(App)))return;
        if(App<5)T->CaptureScreen(FPaths::GetPath(Report)/FString::Printf(TEXT("phone-app-%d.png"),App));Capture(FString::Printf(TEXT("app-%d-world"),App));
        if(App==5){G->PhotoMode->Close();T->Close();Prepare(G);return;}
        if(!Check(T->PreviewTap(360,1390),TEXT("home gesture bar touch ")+FString::FromInt(App)))return;
        ++App;Next(3);return;
    }
    if(Phase==3 && Age>1){if(!Check(T->PageIndex()==0,TEXT("home bar returns to icon grid ")+FString::FromInt(App)))return;Next(1);return;}
    const auto C=EWSky92Plan::Coord(Site);
    auto FindLift=[&]()->AEWLift*{for(TActorIterator<AEWLift> It(GetWorld());It;++It)if(It->Spec.Id==TEXT("sky92/")+C.Text()+TEXT("/lift"))return *It;return nullptr;};
    auto Advance=[&]{++Point;Next(Point==Route.Num()?20:11);};
    if(Phase==10 && Age>2 && Move->IsMovingOnGround())
    {
        PC->SetViewTarget(P);G->SetMenu(EEWMenu::None);Move->MaxWalkSpeed=480;
        LastCoord=M->PlayerCoord();Last=M->ToLocal(LastCoord,P->GetActorLocation());Next(11);return;
    }
    if(Phase==11)
    {
        const auto Pos=P->GetActorLocation();Walked+=FVector::Dist2D(Pos,M->ToRender(LastCoord,Last));LastCoord=M->PlayerCoord();Last=M->ToLocal(LastCoord,Pos);
        const auto Goal=M->ToRender(C,Route[Point].Local)+FVector(0,0,90);const auto D=Goal-Pos;const double Distance=D.Size2D();
        if(Distance>28)
        {
            if(Distance<Best-5){Best=Distance;Progress=Now;}else if(Now-Progress>10){Capture(TEXT("blocked"));Finish(FString::Printf(TEXT("blocked site %d point %d pos %s goal %s"),Site,Point,*M->ToLocal(C,Pos).ToString(),*Route[Point].Local.ToString()));return;}
            PC->SetControlRotation(FRotator(0,D.Rotation().Yaw,0));P->SetTestMovement(D.GetSafeNormal2D(),true);return;
        }
        P->SetTestMovement(FVector::ZeroVector,false);
        if(!Move->IsMovingOnGround())return;
        if(!Check(FMath::Abs(Pos.Z-Goal.Z)<32 && P->FallRecoveries==0,FString::Printf(TEXT("grounded physical walk site %d point %d"),Site,Point)))return;
        const int Action=Route[Point].Action;
        if(Action==1){auto* L=FindLift();if(!Check(L && L->Call(0),TEXT("call real lift ")+FString::FromInt(Site)))return;Next(12);}
        else if(Action==2){auto* L=FindLift();if(!Check(L && L->Ride(P,1),TEXT("board and ride real lift ")+FString::FromInt(Site)))return;Next(13);}
        else Advance();return;
    }
    if(Phase==12){auto* L=FindLift();if(L && !L->IsMoving() && L->FloorIndex()==0)Advance();return;}
    if(Phase==13){auto* L=FindLift();if(L && !L->IsMoving() && L->FloorIndex()==1){LastCoord=M->PlayerCoord();Last=M->ToLocal(LastCoord,P->GetActorLocation());Advance();}return;}
    if(Phase==20 && Age>3)
    {
        Capture(FString::Printf(TEXT("landmark-%d-arrival"),Site));
        if(!Check(P->FallRecoveries==0 && Move->IsMovingOnGround(),TEXT("walkable landmark interior ")+FString::FromInt(Site)))return;
        if(++Site<3){Prepare(G);return;}
        if(!Check(G->SaveNow(),TEXT("existing position and separate life data save")))return;Finish();
    }
}
