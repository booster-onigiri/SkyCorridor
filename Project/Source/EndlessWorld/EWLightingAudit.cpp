#include "EWLightingAudit.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWDayCycle.h"
#include "EWCharacter.h"
#include "EWWaterView.h"
#include "EWPhotoMode.h"
#include "EWSkyTheatrePlan.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "ImageUtils.h"
#include "Math/Float16Color.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
struct FClockShot {const TCHAR* Name;double Hour,Wait;bool Plaza;bool Top=false;};
// Cross the horizon at real clock speed. Jumping straight from night to 07:00
// bypasses the light-registration race this regression must exercise.
const FClockShot Shots[]={
    {TEXT("roof-night"),23,5,false},
    {TEXT("roof-natural-dawn"),5.98,25,false},
    {TEXT("roof-morning"),7.2,5,false},
    {TEXT("plaza-arrival"),-1,.3,true},
    {TEXT("plaza-settled"),-1,5,true},
    {TEXT("roof-noon"),12,5,false},
    {TEXT("roof-evening"),17,5,false},
    {TEXT("roof-natural-dusk"),17.98,20,false},
    {TEXT("plaza-night"),23,5,true},
    {TEXT("roof-second-natural-dawn"),5.98,25,false},
    {TEXT("roof-second-morning"),7.2,5,false},
    {TEXT("sky-morning"),7.2,5,false,true},
    {TEXT("sky-noon"),12,5,false,true},
    {TEXT("sky-evening"),17,5,false,true},
    {TEXT("sky-night"),23,5,false,true}};
}
AEWLightingAudit::AEWLightingAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWLightingAudit::BeginPlay()
{
    Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();int32 Mode=0;FParse::Value(FCommandLine::Get(),TEXT("EWHDR="),Mode);HDR=Mode!=0;
    Resume=FParse::Param(FCommandLine::Get(),TEXT("EWLighting91Resume"));
    if(!FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report) || IFileManager::Get().FileExists(*Report)){Done=true;FPlatformMisc::RequestExit(false);}
}
void AEWLightingAudit::EndPlay(const EEndPlayReason::Type Reason)
{
    UGameViewportClient::OnScreenshotCaptured().Remove(SDRHandle);UGameViewportClient::OnHDRScreenshotCaptured().Remove(HDRHandle);Super::EndPlay(Reason);
}
void AEWLightingAudit::Finish(const FString& Error)
{
    if(Done)return;Done=true;auto* G=GetGameInstance<UEWGameInstance>();
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);
    O->SetBoolField(TEXT("hdr_requested"),HDR);O->SetBoolField(TEXT("resume"),Resume);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);O->SetArrayField(TEXT("captures"),Captures);
    if(G){G->SaveNow();O->SetObjectField(TEXT("lighting"),G->DayCycle->Evidence());O->SetObjectField(TEXT("graphics"),G->Graphics.Evidence());}
    O->SetStringField(TEXT("scope"),TEXT("Two real-speed sunrises, sunset and rooftop/plaza travel through the normal 30-minute clock path. No fixed-hour preview or faster Lumen audit settings. HDR pixels are linear scRGB, 1=80 nits; not a monitor luminance measurement."));
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);FPlatformMisc::RequestExit(false);
}
void AEWLightingAudit::WritePNG(int32 W,int32 H,const TArray<FColor>& Pixels)
{
    TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(W,H,TArrayView64<const FColor>(Pixels),PNG);
    if(PNG.IsEmpty() || !FFileHelper::SaveArrayToFile(PNG,*Pending)){Finish(TEXT("screenshot write failed"));return;}
    Shot->SetNumberField(TEXT("width"),W);Shot->SetNumberField(TEXT("height"),H);Captures.Add(MakeShared<FJsonValueObject>(Shot));Pending.Reset();Phase=3;Stage=FPlatformTime::Seconds();
}
void AEWLightingAudit::CaptureSDR(int32 W,int32 H,const TArray<FColor>& Pixels)
{
    if(Pending.IsEmpty())return;
    if(HDR || int64(W)*H!=Pixels.Num()){Finish(TEXT("unexpected SDR capture"));return;}WritePNG(W,H,Pixels);
}
void AEWLightingAudit::CaptureHDR(int32 W,int32 H,const TArray<FLinearColor>& Pixels)
{
    if(Pending.IsEmpty())return;
    if(!HDR || int64(W)*H!=Pixels.Num()){Finish(TEXT("unexpected HDR capture"));return;}
    TArray<float> Luma;TArray<FFloat16Color> Raw;TArray<FColor> Preview;
    Luma.Reserve(Pixels.Num());Raw.Reserve(Pixels.Num());Preview.Reserve(Pixels.Num());
    for(const auto& C:Pixels)
    {
        if(!FMath::IsFinite(C.R) || !FMath::IsFinite(C.G) || !FMath::IsFinite(C.B)){Finish(TEXT("nonfinite HDR pixel"));return;}
        Raw.Add(FFloat16Color(C));Preview.Add(EWPhoto::ToSDR(C));Luma.Add(FMath::Max(0.f,80.f*(.2126f*C.R+.7152f*C.G+.0722f*C.B)));
    }
    Luma.Sort();for(int32 P:{50,95,99})Shot->SetNumberField(FString::Printf(TEXT("luminance_p%d_nits"),P),Luma[FMath::Min(Luma.Num()-1,int32(Luma.Num()*(P/100.)))]);
    Shot->SetNumberField(TEXT("luminance_max_nits"),Luma.Last());
    const auto File=FPaths::ChangeExtension(Pending,TEXT("rgba16f"));
    if(!FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(Raw.GetData()),Raw.Num()*sizeof(FFloat16Color)),*File)){Finish(TEXT("HDR capture write failed"));return;}
    Shot->SetStringField(TEXT("raw_file"),FPaths::GetCleanFilename(File));WritePNG(W,H,Preview);
}
void AEWLightingAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>320 || Age>70){Finish(FString::Printf(TEXT("timeout shot %d phase %d"),Index,Phase));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !M || !P || !PC || M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt({0,0}))return;
    if(!G->bScriptedWorldAudit){Finish(TEXT("input isolation absent"));return;}
    auto L=G->DayCycle->Evidence();
    if(!L->GetBoolField(TEXT("auxiliary_atmosphere_disabled")) || !L->GetBoolField(TEXT("sun_atmosphere_slot_valid")) || !L->GetBoolField(TEXT("moon_atmosphere_slot_valid")))
    {Finish(TEXT("atmosphere slots are not exclusive to sun and moon"));return;}
    if(L->GetBoolField(TEXT("audit_override")) || L->GetNumberField(TEXT("lumen_scene_update_speed"))!=1 || L->GetNumberField(TEXT("lumen_gather_update_speed"))!=1)
    {Finish(TEXT("ordinary clock or Lumen update path was bypassed"));return;}
    const auto& S=Shots[Index];
    if(Phase==0)
    {
        if(Resume){if(!G->CanContinue()){Finish(TEXT("saved world missing on restart"));return;}G->ContinueWorld();Phase=10;Stage=Now;return;}
        if(!SDRHandle.IsValid())SDRHandle=UGameViewportClient::OnScreenshotCaptured().AddUObject(this,&AEWLightingAudit::CaptureSDR);
        if(!HDRHandle.IsValid())HDRHandle=UGameViewportClient::OnHDRScreenshotCaptured().AddUObject(this,&AEWLightingAudit::CaptureHDR);
        const auto R=M->RecipeAt({0,0});const auto* Part=R->Parts.FindByPredicate([](const auto& V){return V.Mesh.ToString().StartsWith(TEXT("UrbanGarden_"));});
        if(!Part){Finish(TEXT("rooftop garden absent"));return;}Garden=Part->Transform;
        if(S.Top && !EWSkyTheatrePlan::Frame(*R,Garden)){Finish(TEXT("highest theatre deck absent"));return;}
        EW::PlaceBookmark B=M->CurrentPosition();B.Coord={0,0};
        B.LocalPosition=S.Plaza?R->Hub+FVector(0,-1500,100):Garden.TransformPosition(S.Top?EWSkyTheatrePlan::Entry():FVector(-650,1700,100));B.Yaw=90;
        G->Visit(B);Phase=1;Stage=Now;return;
    }
    if(Phase==1 && Age>1)
    {
        const auto R=M->RecipeAt({0,0});FVector Eye,Aim;
        if(S.Plaza){Eye=R->Hub+FVector(0,-1500,164);Aim=R->Hub+FVector(0,0,800);}
        else if(S.Top){Eye=Garden.TransformPosition(FVector(-2050,1500,170));Aim=Garden.TransformPosition(FVector(-12000,-16000,-2200));}
        else {Eye=Garden.TransformPosition(FVector(-650,1700,164));Aim=Garden.TransformPosition(FVector(1500,-900,680));}
        Eye=M->ToRender({0,0},Eye);Aim=M->ToRender({0,0},Aim);
        if(!Camera)Camera=GetWorld()->SpawnActor<ACameraActor>();
        Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(85);PC->SetViewTarget(Camera);G->SetMenu(EEWMenu::None);
        if(S.Hour>=0 && !G->DayCycle->PreviewClock(S.Hour)){Finish(TEXT("clock transition rejected"));return;}
        Phase=2;Stage=Now;return;
    }
    if(Phase==2 && Age>S.Wait && Pending.IsEmpty())
    {
        if(G->Graphics.IsHDRActive()!=HDR){Finish(TEXT("actual DXGI HDR state mismatch"));return;}
        if(G->WaterView->Evidence()->GetNumberField(TEXT("weight"))!=0){Finish(TEXT("water optics leaked outside water"));return;}
        Pending=FPaths::GetPath(Report)/(FString(S.Name)+TEXT(".png"));Shot=MakeShared<FJsonObject>();
        Shot->SetStringField(TEXT("scene"),S.Name);Shot->SetStringField(TEXT("file"),FPaths::GetCleanFilename(Pending));Shot->SetObjectField(TEXT("lighting"),G->DayCycle->Evidence());
        Shot->SetObjectField(TEXT("graphics"),G->Graphics.Evidence());Shot->SetObjectField(TEXT("water"),G->WaterView->Evidence());
        FScreenshotRequest::RequestScreenshot(Pending,false,false,HDR);return;
    }
    if(Phase==3 && Age>.25){if(++Index==UE_ARRAY_COUNT(Shots)){Finish();return;}Phase=0;Stage=Now;}
    if(Phase==10 && Age>4){Finish();}
}
