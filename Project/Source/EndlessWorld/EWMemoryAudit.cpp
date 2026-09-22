#include "EWMemoryAudit.h"
#include "EWGameInstance.h"
#include "EWTerminal.h"
#include "EWMusic.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWDayCycle.h"
#include "EWBrowserSurface.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

AEWMemoryAudit::AEWMemoryAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWMemoryAudit::BeginPlay()
{Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);Resume=FParse::Param(FCommandLine::Get(),TEXT("EWMemoryResume"));}
void AEWMemoryAudit::Capture(const FString& Name){FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/Name+TEXT(".png"),false,false);}
void AEWMemoryAudit::Next(int32 Value)
{
    Phase=Value;Stage=FPlatformTime::Seconds();FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"phase\":%d,\"memory\":%d,\"seconds\":%.2f}"),Phase,Index,Stage-Started),*(Report+TEXT(".progress.json")));
}
bool AEWMemoryAudit::Check(bool Pass,const FString& Name)
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("pass"),Pass);O->SetStringField(TEXT("check"),Name);Checks.Add(MakeShared<FJsonValueObject>(O));if(!Pass)Finish(Name);return Pass;
}
void AEWMemoryAudit::Finish(const FString& Error)
{
    if(Done)return;Done=true;auto* G=GetGameInstance<UEWGameInstance>();
    if(auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))P->SetTestMovement(FVector::ZeroVector,false);
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);O->SetArrayField(TEXT("checks"),Checks);
    O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);O->SetBoolField(TEXT("resume"),Resume);
    if(G && G->Terminal)O->SetObjectField(TEXT("terminal"),G->Terminal->Evidence());if(G && G->Music)O->SetObjectField(TEXT("music"),G->Music->Evidence());
    if(auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))O->SetNumberField(TEXT("fall_recoveries"),P->FallRecoveries);
    O->SetStringField(TEXT("scope"),TEXT("Isolated silent fixture; normal menu/observation/record and character walk from authored nearby approach, not full inter-destination walking or physical speaker acceptance."));
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);FPlatformMisc::RequestExit(false);
}
void AEWMemoryAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;const double Age=FPlatformTime::Seconds()-Stage;
    if(FPlatformTime::Seconds()-Started>540 || Age>70){Capture(TEXT("failure"));Finish(FString::Printf(TEXT("timeout phase %d memory %d"),Phase,Index));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(!G || !G->Terminal || !G->Music || !G->Manager || !P)return;
    auto* T=G->Terminal.Get();auto* M=G->Manager.Get();auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt(M->PlayerCoord()) || T->Places().Num()!=15)return;
    auto* Move=P->GetCharacterMovement();
    if(Phase==0)
    {
        if(!Check(T->Evidence()->GetBoolField(TEXT("journal_integrity")) && T->Places().Num()==15,TEXT("fifteen places and separate journal initialized")))return;
        if(!Check(G->Music->Evidence()->GetIntegerField(TEXT("tracks_loaded"))==3,TEXT("three complete music assets loaded")))return;
        if(Resume){if(!Check(T->RecordCount()==15,TEXT("all fifteen moments survive restart")))return;T->Open();T->ShowPage(2);Next(80);return;}
        if(!Check(T->RecordCount()==0 && !T->RecordNearby(),TEXT("fresh journal; observation is required before recording")))return;
        G->Music->PreviewCue(0);T->Open();Next(1);return;
    }
    if(Phase==1 && Age>4)
    {
        if(!Check(G->Menu()==EEWMenu::Terminal && T->Evidence()->GetNumberField(TEXT("raised"))>.99 && T->Evidence()->GetBoolField(TEXT("screen_render_target")),TEXT("physical handheld screen raises and renders")))return;
        if(!Check(T->Evidence()->GetBoolField(TEXT("hardware_input")),TEXT("world screen accepts hardware pointer input")))return;
        T->CaptureScreen(FPaths::GetPath(Report)/TEXT("terminal-home-surface.png"));Capture(TEXT("terminal-home"));Next(2);return;
    }
    if(Phase==2 && Age>1){T->PreviewTap(520,300);Next(3);return;}
    if(Phase==3 && Age>2)
    {
        if(!Check(T->PageIndex()==1,TEXT("projected pointer reaches the real map button through Slate")))return;
        Capture(TEXT("terminal-map"));T->CaptureScreen(FPaths::GetPath(Report)/TEXT("terminal-map-surface.png"));Next(4);return;
    }
    if(Phase==4 && Age>1){T->Close();T->PreviewAt(0);Next(10);return;}
    if(Phase==10 && Age>2 && Move->IsMovingOnGround())
    {
        if(!Check(P->FallRecoveries==0,TEXT("memory approach on existing floor ")+FString::FromInt(Index)))return;
        const auto& A=T->Places()[Index];const FVector Foot=M->ToRender(A.Place.Coord,A.Place.LocalPosition);
        FHitResult Floor;FCollisionQueryParams Query;Query.AddIgnoredActor(T);Query.AddIgnoredActor(P);
        if(!Check(GetWorld()->LineTraceSingleByChannel(Floor,Foot+FVector(0,0,70),Foot-FVector(0,0,120),ECC_Visibility,Query) && FMath::Abs(Floor.ImpactPoint.Z-Foot.Z)<40,TEXT("trace grounded on real floor ")+FString::FromInt(Index)))return;
        if(!Check(T->ObservedSecondsForAudit()<2,TEXT("distant approach cannot record yet ")+FString::FromInt(Index)))return;
        InitialRecords=T->RecordCount();Next(11);return;
    }
    if(Phase==11)
    {
        const auto& A=T->Places()[Index];const FVector Foot=M->ToRender(A.Place.Coord,A.Place.LocalPosition);
        const FVector Dir=(A.Place.LocalPosition-A.Approach).GetSafeNormal2D();const FVector Target=Foot-Dir*190+FVector(0,0,88);
        PC->SetControlRotation((Foot+FVector(0,0,108)-P->Camera->GetComponentLocation()).Rotation());
        const FVector D=Target-P->GetActorLocation();
        if(D.Size2D()>35){P->SetTestMovement(D,true);return;}
        P->SetTestMovement(FVector::ZeroVector,false);
        if(T->ObservedSecondsForAudit()<2.1)return;
        G->Interact();
        if(!Check(T->Recorded(Index) && T->RecordCount()==InitialRecords+1,TEXT("walk, observe and save moment ")+FString::FromInt(Index)))return;
        G->Interact();if(!Check(T->RecordCount()==InitialRecords+1,TEXT("repeat recording does not duplicate ")+FString::FromInt(Index)))return;
        Capture(FString::Printf(TEXT("memory-%02d"),Index));Next(12);return;
    }
    if(Phase==12 && Age>1)
    {
        if(++Index<15){T->PreviewAt(Index);Next(10);return;}
        T->Open();T->ShowPage(2);Next(70);return;
    }
    if(Phase==70 && Age>3)
    {T->CaptureScreen(FPaths::GetPath(Report)/TEXT("journal-surface.png"));Capture(TEXT("journal-complete"));Next(71);return;}
    if(Phase==71 && Age>1)
    {
        if(!Check(T->RecordCount()==15 && T->Evidence()->GetBoolField(TEXT("journal_integrity")) && G->SaveNow(),TEXT("all narrative and old exploration stores save independently")))return;
        G->Music->SetVolume(0);if(!Check(G->Music->Evidence()->GetIntegerField(TEXT("current"))==-1,TEXT("music off stops the active cue immediately")))return;
        G->Music->SetVolume(.32f);Finish();return;
    }
    if(Phase==80 && Age>3){Capture(TEXT("journal-restored"));Finish();return;}
}
