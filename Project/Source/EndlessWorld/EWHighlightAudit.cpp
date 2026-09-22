#include "EWHighlightAudit.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWDayCycle.h"
#include "EWHotelPlan.h"
#include "EWLightingViews.h"
#include "EWMediaScreen.h"
#include "EWPhotoMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "Math/Float16Color.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace
{
struct FShot {const TCHAR* Name;int32 Room,View;double Hour;};
// Living room, sunlit terrace, shaded plaza/gallery, night and auditorium.
const FShot Shots[]={
    {TEXT("upper-terrace-noon"),7,2,12},{TEXT("water-salon-morning"),2,0,7},
    {TEXT("pearl-salon-noon"),4,0,12},{TEXT("japanese-suite-evening"),0,0,17},
    {TEXT("terrace-morning"),2,2,7},{TEXT("plaza-noon"),-1,1,12},
    {TEXT("gallery-noon"),-1,2,12},{TEXT("plaza-morning"),-1,0,7},
    {TEXT("plaza-evening"),-1,0,17},{TEXT("plaza-night"),-1,0,23},
    {TEXT("water-salon-night"),2,0,23},{TEXT("cinema-noon"),-2,0,12}};
}
AEWHighlightAudit::AEWHighlightAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWHighlightAudit::BeginPlay()
{
    Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();int32 HDR=0;FParse::Value(FCommandLine::Get(),TEXT("EWHDR="),HDR);WantHDR=HDR!=0;
    if(!FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report) || IFileManager::Get().FileExists(*Report))
    {Done=true;FPlatformMisc::RequestExit(false);return;}
    // Sweep every second of a game day, including the old sunrise blowout.
    for(int32 Second=0;Second<86400;++Second)
    {
        const auto L=AEWDayCycle::Evaluate(Second/3600.);
        if(!FMath::IsFinite(L.Exposure) || L.Exposure<6.299f || L.Exposure>13.201f ||
           L.SunLux/FMath::Pow(2.f,L.Exposure)>11.70f)
        {Finish(TEXT("daylight exposure lost highlight headroom"));return;}
    }
    const auto Night=AEWDayCycle::Evaluate(23),OldNight=AEWDayCycle::Evaluate(23,true);
    if(Night.Exposure!=OldNight.Exposure || Night.Emission!=OldNight.Emission || Night.MoonLux!=OldNight.MoonLux)
        Finish(TEXT("night lighting anchor changed"));
}
void AEWHighlightAudit::EndPlay(const EEndPlayReason::Type Reason)
{
    UGameViewportClient::OnScreenshotCaptured().Remove(SDRHandle);
    UGameViewportClient::OnHDRScreenshotCaptured().Remove(HDRHandle);Super::EndPlay(Reason);
}
void AEWHighlightAudit::Finish(const FString& Error)
{
    if(Done)return;Done=true;
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);
    O->SetBoolField(TEXT("hdr_requested"),WantHDR);O->SetBoolField(TEXT("legacy"),FParse::Param(FCommandLine::Get(),TEXT("EWHighlight84Legacy")));
    O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);O->SetArrayField(TEXT("captures"),Captures);
    O->SetNumberField(TEXT("exposure_sweep_samples"),86400);O->SetBoolField(TEXT("night_anchor_preserved"),true);
    O->SetStringField(TEXT("hdr_pixels"),TEXT("UE viewport converted to linear scRGB; 1.0 = 80 nits. PNG is an SDR preview, not a display measurement."));
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);FFileHelper::SaveStringToFile(Text,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExit(false);
}
void AEWHighlightAudit::WritePNG(int32 W,int32 H,const TArray<FColor>& Pixels)
{
    TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(W,H,TArrayView64<const FColor>(Pixels),PNG);
    if(PNG.IsEmpty() || !FFileHelper::SaveArrayToFile(PNG,*Pending)){Finish(TEXT("screenshot write failed"));return;}
    Shot->SetNumberField(TEXT("width"),W);Shot->SetNumberField(TEXT("height"),H);
    Captures.Add(MakeShared<FJsonValueObject>(Shot));Pending.Reset();Phase=3;Stage=FPlatformTime::Seconds();
}
void AEWHighlightAudit::CaptureSDR(int32 W,int32 H,const TArray<FColor>& Pixels)
{
    if(Pending.IsEmpty())return;
    if(WantHDR || int64(W)*H!=Pixels.Num()){Finish(TEXT("expected HDR float pixels but received SDR"));return;}
    Shot->SetBoolField(TEXT("hdr_float_capture"),false);WritePNG(W,H,Pixels);
}
void AEWHighlightAudit::CaptureHDR(int32 W,int32 H,const TArray<FLinearColor>& Pixels)
{
    if(Pending.IsEmpty())return;
    if(!WantHDR || int64(W)*H!=Pixels.Num()){Finish(TEXT("unexpected HDR capture dimensions or mode"));return;}
    TArray<float> Luma;TArray<FFloat16Color> Raw;TArray<FColor> Preview;
    Luma.Reserve(Pixels.Num());Raw.Reserve(Pixels.Num());Preview.Reserve(Pixels.Num());
    for(const auto& C:Pixels)
    {
        if(!FMath::IsFinite(C.R) || !FMath::IsFinite(C.G) || !FMath::IsFinite(C.B)){Finish(TEXT("nonfinite HDR pixel"));return;}
        Raw.Add(FFloat16Color(C));Preview.Add(EWPhoto::ToSDR(C));Luma.Add(FMath::Max(0.f,80.f*(.2126f*C.R+.7152f*C.G+.0722f*C.B)));
    }
    Luma.Sort();Shot->SetBoolField(TEXT("hdr_float_capture"),true);
    for(int32 P:{50,95,99})Shot->SetNumberField(FString::Printf(TEXT("luminance_p%d_nits"),P),Luma[FMath::Min(Luma.Num()-1,int32(Luma.Num()*(P/100.)))]);
    Shot->SetNumberField(TEXT("luminance_max_nits"),Luma.Last());
    const FString RawFile=FPaths::ChangeExtension(Pending,TEXT("rgba16f"));
    if(!FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(Raw.GetData()),Raw.Num()*sizeof(FFloat16Color)),*RawFile))
    {Finish(TEXT("HDR raw capture write failed"));return;}
    Shot->SetStringField(TEXT("raw_file"),FPaths::GetCleanFilename(RawFile));WritePNG(W,H,Preview);
}
void AEWHighlightAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>300 || Age>40){Finish(FString::Printf(TEXT("timeout case %d phase %d"),Index,Phase));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !M || !P || !PC || M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt({0,0}))return;
    const auto& S=Shots[Index];
    if(Phase==0)
    {
        if(!SDRHandle.IsValid())SDRHandle=UGameViewportClient::OnScreenshotCaptured().AddUObject(this,&AEWHighlightAudit::CaptureSDR);
        if(!HDRHandle.IsValid())HDRHandle=UGameViewportClient::OnHDRScreenshotCaptured().AddUObject(this,&AEWHighlightAudit::CaptureHDR);
        PC->SetViewTarget(P);
        if(S.Room>=0)G->VisitSkyResidence(S.Room);
        else if(S.Room==-2)G->VisitCinema();
        else
        {
            EW::PlaceBookmark Place;Place.WorldCode=EW::WorldDescriptor::ReferenceWorld().Code();Place.Coord={0,0};
            Place.LocalPosition=M->RecipeAt({0,0})->Hub+FVector(0,-1250,100);Place.Yaw=90;Place.Name=TEXT("highlight audit plaza");G->Visit(Place);
        }
        G->DayCycle->SetAuditHour(S.Hour);Phase=1;Stage=Now;return;
    }
    if(Phase==1 && Age>2)
    {
        const auto R=M->RecipeAt({0,0});if(!R)return;
        FVector Eye,Aim;FRotator Rotation;
        if(S.Room>=0)
        {
            const auto* Room=EWHotelPlan::Find(*R,S.Room);if(!Room){Finish(TEXT("suite absent"));return;}
            const auto T=Room->Frame;
            Eye=M->ToRender({0,0},T.TransformPosition(S.View==2?FVector(0,820,170):FVector(-110,-425,165)));
            Aim=M->ToRender({0,0},T.TransformPosition(S.View==2?FVector(250,50,135):FVector(-350,180,145)));Rotation=(Aim-Eye).Rotation();
        }
        else if(S.Room==-2)
        {
            if(!G->CinemaScreen->Available())return;const auto T=G->CinemaScreen->RoomFrame();
            Eye=T.TransformPosition(FVector(0,400,460));Aim=T.TransformPosition(FVector(0,-1030,580));Rotation=(Aim-Eye).Rotation();
            P->SetActorLocation(Eye-FVector(0,0,70),false);P->GetCharacterMovement()->DisableMovement();
        }
        else {const auto T=EWLighting::PreviewView(*R,S.View);Eye=M->ToRender({0,0},T.GetLocation());Rotation=T.Rotator();}
        if(!Camera)Camera=GetWorld()->SpawnActor<ACameraActor>();
        Camera->SetActorLocationAndRotation(Eye,Rotation);Camera->GetCameraComponent()->SetFieldOfView(80);
        PC->SetViewTarget(Camera);G->SetMenu(EEWMenu::None);Phase=2;Stage=Now;return;
    }
    if(Phase==2 && Age>5 && Pending.IsEmpty())
    {
        if(G->Graphics.IsHDRActive()!=WantHDR){Finish(TEXT("native DXGI output does not match requested HDR/SDR"));return;}
        Pending=FPaths::GetPath(Report)/(FString(S.Name)+TEXT(".png"));Shot=MakeShared<FJsonObject>();
        Shot->SetStringField(TEXT("scene"),S.Name);Shot->SetStringField(TEXT("file"),FPaths::GetCleanFilename(Pending));
        Shot->SetObjectField(TEXT("lighting"),G->DayCycle->Evidence());Shot->SetObjectField(TEXT("graphics"),G->Graphics.Evidence());
        FScreenshotRequest::RequestScreenshot(Pending,false,false,WantHDR);return;
    }
    if(Phase==3 && Age>.25)
    {
        if(++Index==UE_ARRAY_COUNT(Shots)){Finish();return;}Phase=0;Stage=Now;
    }
}
