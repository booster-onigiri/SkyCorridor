#include "EWCity82Audit.h"
#include "EWGameInstance.h"
#include "EWConceptRuntime.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWMediaScreen.h"
#include "EWBrowserSurface.h"
#include "EWAirship.h"
#include "EWSkyrail.h"
#include "EWSkyTheatrePlan.h"
#include "EWCinemaPlan.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "RHI.h"

AEWCity82Audit::AEWCity82Audit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWCity82Audit::BeginPlay()
{Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);if(Report.IsEmpty())Finish(TEXT("explicit report path required"));}
void AEWCity82Audit::Capture(const FString& N){if(!GUsingNullRHI)FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/N+TEXT(".png"),N==TEXT("chess-interface"),false);}
bool AEWCity82Audit::Check(bool OK,const FString& Name)
{auto C=MakeShared<FJsonObject>();C->SetBoolField(TEXT("pass"),OK);C->SetStringField(TEXT("check"),Name);Checks.Add(MakeShared<FJsonValueObject>(C));if(!OK)Finish(Name);return OK;}
void AEWCity82Audit::Finish(const FString& Error)
{
    if(Finished)return;Finished=true;auto* G=GetGameInstance<UEWGameInstance>();auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);O->SetArrayField(TEXT("checks"),Checks);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    if(G && G->Concepts)O->SetObjectField(TEXT("workshop"),G->Concepts->Evidence());if(G && G->Airship)O->SetObjectField(TEXT("airship"),G->Airship->Evidence());
    if(G && G->CinemaScreen)O->SetObjectField(TEXT("cinema"),G->CinemaScreen->Evidence());if(G && G->SkyTheatre)O->SetObjectField(TEXT("sky"),G->SkyTheatre->Evidence());
    if(auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))P->SetTestMovement(FVector::ZeroVector,false);
    O->SetStringField(TEXT("scope"),TEXT("real local gameplay and silent visual fixtures; no EOS or physical audio acceptance"));
    FString T;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&T));if(!Report.IsEmpty()){IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);FFileHelper::SaveStringToFile(T,*Report);}FPlatformMisc::RequestExit(false);
}
void AEWCity82Audit::Tick(float Delta)
{
    Super::Tick(Delta);if(Finished)return;const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>230){Finish(FString::Printf(TEXT("timeout phase %d"),Phase));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !G->Manager || !G->Concepts || !P || !PC)return;auto* M=G->Manager.Get();auto* C=G->Concepts.Get();
    if(M->IsTravelling() || P->IsStreamingHeld() || M->ReadyCount()<49)return;
    auto Next=[&](int I){Phase=I;Stage=Now;UE_LOG(LogTemp,Display,TEXT("CITY82_STAGE %d"),I);};
    if(Phase==0){G->ReturnToPlaza();Next(1);return;}
    if(Phase==1 && Age>3)
    {
        bool Added=false;for(int I=0;I<8 && !Added;++I){PC->SetControlRotation(FRotator(0,I*45.,0));Added=C->Add(TEXT("chess-garden.concept.json"));}
        if(!Check(Added && C->Active() && C->Evidence()->GetNumberField(TEXT("meshes"))>100,TEXT("package creates complete colliding chess table in front of player")))return;
        PC->SetControlRotation((C->GetActorLocation()+FVector(0,0,95)-(P->GetActorLocation()+FVector(0,0,64))).Rotation());
        if(FParse::Param(FCommandLine::Get(),TEXT("EWCity82Resume")))
        {if(!Check(C->Match().Moves.size()==2 && C->Match().At.Board[28]==1 && C->Match().At.Board[36]==-1,TEXT("fresh process restores saved moves by legal replay")))return;Next(20);return;}
        Next(2);return;
    }
    if(Phase==2 && Age>3){Capture(TEXT("chess-world"));Next(3);return;}
    if(Phase==3 && Age>1)
    {
        G->Interact();if(!Check(G->Menu()==EEWMenu::Chess,TEXT("normal E interaction opens playable chess interface")))return;
        if(!Check(!C->Play(TEXT("e2e5")) && C->Play(TEXT("e2e4")) && C->Play(TEXT("e7e5")) && C->Undo() && C->Play(TEXT("e7e5")),TEXT("legal moves and undo use transactional local saves")))return;
        G->RefreshUI();Next(4);return;
    }
    if(Phase==4 && Age>2){Capture(TEXT("chess-interface"));Next(5);return;}
    if(Phase==5 && Age>1)
    {
        G->SetMenu(EEWMenu::None);C->Remove();
        TArray<UStaticMeshComponent*> Meshes;C->GetComponents(Meshes);
        if(!Check(!C->Active() && Meshes.IsEmpty(),TEXT("removal destroys package geometry")))return;
        if(!Check(C->Add(TEXT("chess-garden.concept.json")) && C->Match().Moves.size()==2,TEXT("reinstallation restores ongoing chess game")))return;
        C->Remove();if(!Check(C->LoadEgg(TEXT("another-sky.egg.json")),TEXT("alternative world loaded through egg file")))return;Next(6);return;
    }
    if(Phase==6 && Age>3)
    {
        if(!Check(M->Descriptor().Code()!=EW::WorldDescriptor::ReferenceWorld().Code() && P->GetCharacterMovement()->IsMovingOnGround(),TEXT("egg arrival reaches generated physical walking floor")))return;
        Capture(TEXT("egg-another-world"));Next(7);return;
    }
    if(Phase==7 && Age>1)
    {if(!Check(C->LoadEgg(TEXT("white-city.egg.json")),TEXT("reference city restored from egg")))return;Next(8);return;}
    if(Phase==8 && Age>2){G->VisitCinema();Next(9);return;}
    if(Phase==9 && Age>4)
    {
        auto* S=G->CinemaScreen.Get();if(!Check(S && S->Available() && S->ScreenWidth()==1850 && S->SitSeat(22),TEXT("18.5 metre cinema screen and actual seat")))return;
        S->LookAtScreen();S->Browser()->TestSyncFilm();Next(10);return;
    }
    if(Phase==10 && Age>5)
    {
        const auto Video=G->CinemaScreen->Evidence();
        if(!Check(Video->GetBoolField(TEXT("video")) && Video->GetNumberField(TEXT("video_time"))>1 && Video->GetNumberField(TEXT("paint_frames"))>10,TEXT("cinema renders actual advancing video frames")))return;
        Capture(TEXT("cinema-large"));Next(11);return;
    }
    if(Phase==11 && Age>1){P->LeaveSeat();G->VisitSkyTheatre();Next(12);return;}
    if(Phase==12 && Age>4)
    {
        auto* S=G->SkyTheatre.Get();if(!Check(S && S->Available() && S->ScreenWidth()==EWSkyTheatrePlan::Width && P->GetCharacterMovement()->IsMovingOnGround(),TEXT("highest theatre has physical entrance and authored screen width")))return;
        const FTransform T=S->RoomFrame();const FVector Destination=T.TransformPosition(FVector(0,-1550,205));
        P->SetTestMovement(Destination-P->GetActorLocation(),true);Next(13);return;
    }
    if(Phase==13)
    {
        const FVector Goal=G->SkyTheatre->RoomFrame().TransformPosition(FVector(0,-1550,205));
        if(FVector::Dist2D(P->GetActorLocation(),Goal)>30){P->SetTestMovement(Goal-P->GetActorLocation(),true);return;}
        P->SetTestMovement(FVector::ZeroVector,false);
        if(!Check(P->FallRecoveries==0 && P->GetCharacterMovement()->IsMovingOnGround(),TEXT("walk up rear theatre stairs without jumping or recovery")))return;
        if(!Check(G->SkyTheatre->SitSeat(27),TEXT("sit in sky theatre back row")))return;
        G->SkyTheatre->LookAtScreen();G->SkyTheatre->Browser()->TestSyncFilm();Next(14);return;
    }
    if(Phase==14 && Age>5)
    {
        const auto Video=G->SkyTheatre->Evidence();
        if(!Check(Video->GetBoolField(TEXT("video")) && Video->GetNumberField(TEXT("video_time"))>1 && Video->GetNumberField(TEXT("paint_frames"))>10,TEXT("sky theatre renders actual advancing video frames")))return;
        Capture(TEXT("sky-theatre-seated"));Next(15);return;
    }
    if(Phase==15 && Age>1)
    {
        if(!Check(P->LeaveSeat(),TEXT("leave sky seat onto terrace")))return;
        ShipBefore=G->Airship->GetActorLocation();Camera=GetWorld()->SpawnActor<ACameraActor>();
        const FVector Eye=ShipBefore+FVector(3300,-4200,650);Camera->SetActorLocationAndRotation(Eye,(ShipBefore-Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(68);PC->SetViewTarget(Camera);
        Next(16);return;
    }
    if(Phase==16 && Age>4)
    {
        if(!Check(G->Airship->Evidence()->GetBoolField(TEXT("ready")) && G->Airship->Evidence()->GetBoolField(TEXT("hull_loaded")) && FVector::Dist(ShipBefore,G->Airship->GetActorLocation())>200,TEXT("authored airship travels through streamed city skyline")))return;
        Capture(TEXT("airship-skyline"));Next(17);return;
    }
    if(Phase==17 && Age>1){PC->SetViewTarget(P);G->VisitSkyrail();Next(18);return;}
    if(Phase==18 && Age>4)
    {
        if(!G->Skyrail->IsReady())return;
        TArray<UBoxComponent*> Bodies;G->Skyrail->GetComponents(Bodies);int Enabled=0,HitCount=0;
        for(auto* B:Bodies)if(B->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
        {
            ++Enabled;FHitResult H;FCollisionQueryParams Q;Q.AddIgnoredActor(P);
            const FVector At=B->GetComponentLocation(),N=B->GetUpVector();
            if(B->LineTraceComponent(H,At+N*(B->GetScaledBoxExtent().Z+15),At-N*(B->GetScaledBoxExtent().Z+15),Q))++HitCount;
        }
        if(!Check(Enabled>=16 && HitCount==Enabled,TEXT("train floor body and moving doors have actual query collision")))return;
        G->Skyrail->PreviewStation(0,true);Next(19);return;
    }
    if(Phase==19 && Age>4){Capture(TEXT("train-redesign"));Next(20);return;}
    if(Phase==20 && Age>2){G->SaveNow();Finish();}
}
