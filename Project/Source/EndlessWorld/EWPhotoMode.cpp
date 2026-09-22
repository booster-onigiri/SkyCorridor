#include "EWPhotoMode.h"
#include "EWGameInstance.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWSocialSession.h"
#include "EWFishing.h"
#include "EWCinemaSession.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealClient.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

FIntRect EWPhoto::Crop(int32 Width,int32 Height,bool Portrait)
{
    if(Width<16 || Height<16 || Width>16384 || Height>16384)return FIntRect();
    const int32 X=Portrait?9:16,Y=Portrait?16:9;
    const int32 Unit=FMath::Min(Width/X,Height/Y);if(Unit<1)return FIntRect();
    const int32 W=Unit*X,H=Unit*Y;
    return FIntRect((Width-W)/2,(Height-H)/2,(Width-W)/2+W,(Height-H)/2+H);
}
FColor EWPhoto::ToSDR(const FLinearColor& Pixel)
{
    auto Map=[](float V){const float X=FMath::IsFinite(V)?FMath::Clamp(V,0.f,100.f):0.f;return FMath::Clamp((X*(2.51f*X+.03f))/(X*(2.43f*X+.59f)+.14f),0.f,1.f);};
    return FLinearColor(Map(Pixel.R),Map(Pixel.G),Map(Pixel.B),1).ToFColorSRGB();
}
AEWPhotoMode::AEWPhotoMode(){PrimaryActorTick.bCanEverTick=true;}
void AEWPhotoMode::Open()
{
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !P || !PC || !G->SessionStarted() || !G->Manager || G->Manager->IsTravelling())return;
    if(bActive){G->SetMenu(EEWMenu::Photo);return;}
    if(G->Fishing)G->Fishing->Cancel();P->ClearHeldInput();
    PreviousView=PC->GetViewTarget();CameraYaw=PC->GetControlRotation().Yaw;CameraPitch=PC->GetControlRotation().Pitch;
    if(!Camera)Camera=GetWorld()->SpawnActor<ACameraActor>();
    if(!SelfAvatar)SelfAvatar=GetWorld()->SpawnActor<AEWCinemaPeer>();
    if(!Camera || !SelfAvatar)return;
    bActive=true;Message.Reset();UpdateCamera();PC->SetViewTarget(Camera);G->SetMenu(EEWMenu::Photo);
}
void AEWPhotoMode::RestoreNames()
{
    for(const auto& Pair:NameVisibility)if(Pair.Key.IsValid())Pair.Key->SetVisibility(Pair.Value);
    NameVisibility.Reset();
}
void AEWPhotoMode::Close()
{
    if(!bActive)return;bActive=false;CaptureAt=0;
    if(!PendingPath.IsEmpty() && FScreenshotRequest::GetFilename()==PendingPath)FScreenshotRequest::Reset();
    PendingPath.Reset();UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle);CaptureHandle.Reset();
    UGameViewportClient::OnHDRScreenshotCaptured().Remove(HDRCaptureHandle);HDRCaptureHandle.Reset();
    RestoreNames();if(SelfAvatar)SelfAvatar->SetActorHiddenInGame(true);
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(PreviousView.IsValid()?PreviousView.Get():PC->GetPawn());
}
void AEWPhotoMode::UpdateCamera()
{
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(!P || !Camera)return;
    auto* G=GetGameInstance<UEWGameInstance>();
    auto* C=Camera->GetCameraComponent();C->SetFieldOfView(FOV);C->SetAspectRatio(bPortrait?9.f/16.f:16.f/9.f);C->bConstrainAspectRatio=true;
    if(bSelfie)
    {
        const FVector Target=P->GetActorLocation()+FVector(0,0,P->IsSeated()?35:20);
        const FVector Desired=Target+FRotator(CameraPitch,CameraYaw,0).Vector()*310.;
        FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(PhotoCamera),false,P);Params.AddIgnoredActor(SelfAvatar);
        FVector Position=Desired;
        if(GetWorld()->SweepSingleByChannel(Hit,Target,Desired,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(12),Params))Position=Hit.Location+Hit.Normal*5;
        Camera->SetActorLocationAndRotation(Position,(Target-Position).Rotation());
        SelfAvatar->UpdatePose(FTransform::Identity,P->GetActorLocation(),CameraYaw,P->IsSeated(),true,
            bNames && G && G->SocialSession?G->SocialSession->Nickname():FString(),1);
    }
    else{Camera->SetActorLocationAndRotation(P->Camera->GetComponentLocation(),FRotator(CameraPitch,CameraYaw,0));SelfAvatar->SetActorHiddenInGame(true);}
    for(TActorIterator<AEWCinemaPeer> It(GetWorld());It;++It)
    {
        TArray<UWidgetComponent*> Widgets;It->GetComponents(Widgets);
        for(auto* W:Widgets)
        {
            const TWeakObjectPtr<UWidgetComponent> Key(W);if(!NameVisibility.Contains(Key))NameVisibility.Add(Key,W->IsVisible());
            W->SetVisibility(bNames && NameVisibility.FindChecked(Key));
        }
    }
}
void AEWPhotoMode::Tick(float Delta)
{
    Super::Tick(Delta);if(!bActive)return;
    const auto* G=GetGameInstance<UEWGameInstance>();
    if(!G || G->Menu()!=EEWMenu::Photo || !G->Manager || G->Manager->IsTravelling()){Close();return;}
    UpdateCamera();const double Now=FPlatformTime::Seconds();
    if(!PendingPath.IsEmpty() && Now-RequestedAt>15)
    {
        if(FScreenshotRequest::GetFilename()==PendingPath)FScreenshotRequest::Reset();
        PendingPath.Reset();UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle);CaptureHandle.Reset();
        UGameViewportClient::OnHDRScreenshotCaptured().Remove(HDRCaptureHandle);HDRCaptureHandle.Reset();
        Message=TEXT("撮影が完了しませんでした。もう一度お試しください。");
    }
    if(CaptureAt<=0 || Now<CaptureAt)return;CaptureAt=0;
    if(FScreenshotRequest::IsScreenshotRequested() || UGameViewportClient::OnScreenshotCaptured().IsBound() || UGameViewportClient::OnHDRScreenshotCaptured().IsBound())
    {Message=TEXT("別の撮影が進行中です。少し待ってからお試しください。");return;}
    if(!G->Store()){Message=TEXT("写真の保存先を開けません。");return;}
    const FString Directory=G->Store()->Root()/TEXT("Screenshots");
    if(!IFileManager::Get().MakeDirectory(*Directory,true)){Message=TEXT("写真の保存先を作成できません。");return;}
    PendingPath=Directory/(TEXT("空の回廊-")+FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S-"))+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)+TEXT(".png"));
    RequestedAt=Now;CaptureHandle=UGameViewportClient::OnScreenshotCaptured().AddUObject(this,&AEWPhotoMode::ReceiveScreenshot);
    HDRCaptureHandle=UGameViewportClient::OnHDRScreenshotCaptured().AddUObject(this,&AEWPhotoMode::ReceiveHDRScreenshot);
    FScreenshotRequest::RequestScreenshot(PendingPath,false,false,false);Message=TEXT("写真を保存しています…");
}
void AEWPhotoMode::Shoot(){if(bActive && !Busy()){CaptureAt=FPlatformTime::Seconds()+TimerSeconds+.15;Message.Reset();}}
void AEWPhotoMode::ReceiveHDRScreenshot(int32 Width,int32 Height,const TArray<FLinearColor>& Pixels)
{
    if(PendingPath.IsEmpty() || FScreenshotRequest::GetFilename()!=PendingPath)return;
    TArray<FColor> Converted;
    if(Width>0 && Height>0 && Width<=16384 && Height<=16384 && int64(Width)*Height==Pixels.Num())
    {Converted.Reserve(Pixels.Num());for(const auto& Pixel:Pixels)Converted.Add(EWPhoto::ToSDR(Pixel));}
    ReceiveScreenshot(Width,Height,Converted);
}
void AEWPhotoMode::ReceiveScreenshot(int32 Width,int32 Height,const TArray<FColor>& Pixels)
{
    if(PendingPath.IsEmpty() || FScreenshotRequest::GetFilename()!=PendingPath)return;
    const auto Rect=EWPhoto::Crop(Width,Height,bPortrait);bool OK=Rect.Width()>0 && int64(Pixels.Num())==int64(Width)*Height;
    TArray<FColor> Cropped;
    if(OK)
    {
        Cropped.SetNumUninitialized(Rect.Width()*Rect.Height());
        for(int32 Y=0;Y<Rect.Height();++Y)for(int32 X=0;X<Rect.Width();++X)
        {auto Colour=Pixels[(Y+Rect.Min.Y)*Width+X+Rect.Min.X];Colour.A=255;Cropped[Y*Rect.Width()+X]=Colour;}
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Rect.Width(),Rect.Height(),TArrayView64<const FColor>(Cropped),PNG);
        const FString Temporary=PendingPath+TEXT(".writing");
        OK=!PNG.IsEmpty() && !IFileManager::Get().FileExists(*PendingPath) && FFileHelper::SaveArrayToFile(PNG,*Temporary) &&
            IFileManager::Get().Move(*PendingPath,*Temporary,false,false,false,true);
    }
    if(OK)
    {
        LastPath=PendingPath;auto Metadata=MakeShared<FJsonObject>();Metadata->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());
        Metadata->SetNumberField(TEXT("width"),Rect.Width());Metadata->SetNumberField(TEXT("height"),Rect.Height());
        Metadata->SetBoolField(TEXT("portrait"),bPortrait);Metadata->SetBoolField(TEXT("selfie"),bSelfie);Metadata->SetBoolField(TEXT("nameplates"),bNames);
        Metadata->SetNumberField(TEXT("fov"),FOV);
        const auto* G=GetGameInstance<UEWGameInstance>();
        if(G && G->Manager){const auto Place=G->Manager->CurrentPosition();Metadata->SetStringField(TEXT("place_code"),Place.Code());}
        const bool MetaOK=FFileHelper::SaveStringToFile(EWSocial::Encode(Metadata),*FPaths::ChangeExtension(LastPath,TEXT("json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        Message=MetaOK?TEXT("写真を保存しました。Screenshots フォルダーから開けます。"):TEXT("写真は保存済みです。撮影場所の記録は保存できませんでした。");
    }
    else Message=TEXT("写真を保存できませんでした。空き容量を確認してください。");
    PendingPath.Reset();UGameViewportClient::OnScreenshotCaptured().Remove(CaptureHandle);CaptureHandle.Reset();
    UGameViewportClient::OnHDRScreenshotCaptured().Remove(HDRCaptureHandle);HDRCaptureHandle.Reset();
}
void AEWPhotoMode::TogglePortrait(){if(!Busy())bPortrait=!bPortrait;}
void AEWPhotoMode::ToggleSelfie(){if(!Busy()){bSelfie=!bSelfie;CameraPitch=bSelfie?8:0;}}
void AEWPhotoMode::ToggleNames(){if(!Busy())bNames=!bNames;}
void AEWPhotoMode::CycleTimer(){if(!Busy())TimerSeconds=TimerSeconds==0?3:TimerSeconds==3?10:0;}
void AEWPhotoMode::SetFieldOfView(float Value){if(!Busy() && FMath::IsFinite(Value))FOV=FMath::Clamp(Value,35.f,100.f);}
void AEWPhotoMode::Turn(float Degrees){if(!Busy() && FMath::IsFinite(Degrees))CameraYaw=FRotator::NormalizeAxis(CameraYaw+FMath::Clamp(Degrees,-45.f,45.f));}
void AEWPhotoMode::Tilt(float Degrees){if(!Busy() && FMath::IsFinite(Degrees))CameraPitch=FMath::Clamp(CameraPitch+FMath::Clamp(Degrees,-30.f,30.f),-70.f,70.f);}
FString AEWPhotoMode::Status() const
{return CaptureAt>0?FString::Printf(TEXT("%d 秒後に撮影"),FMath::Clamp(FMath::CeilToInt(CaptureAt-FPlatformTime::Seconds()),1,FMath::Max(1,TimerSeconds))):Message;}
void AEWPhotoMode::EndPlay(const EEndPlayReason::Type Reason){Close();if(Camera)Camera->Destroy();if(SelfAvatar)SelfAvatar->Destroy();Super::EndPlay(Reason);}
FString AEWPhotoMode::PhotoEvidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("active"),bActive);O->SetBoolField(TEXT("portrait"),bPortrait);O->SetBoolField(TEXT("selfie"),bSelfie);
    O->SetBoolField(TEXT("names"),bNames);O->SetBoolField(TEXT("busy"),Busy());O->SetNumberField(TEXT("timer"),TimerSeconds);O->SetNumberField(TEXT("fov"),FOV);
    O->SetStringField(TEXT("last_file"),LastPath);O->SetStringField(TEXT("status"),Status());O->SetBoolField(TEXT("saved_file_exists"),!LastPath.IsEmpty() && IFileManager::Get().FileSize(*LastPath)>0);
    if(bActive && bSelfie && Camera)
    {
        if(auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))
        {
            FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(SelfieVisibility),false,P);Q.AddIgnoredActor(SelfAvatar);Q.AddIgnoredActor(Camera);
            const bool Hidden=GetWorld()->LineTraceSingleByChannel(Hit,Camera->GetActorLocation(),P->GetActorLocation()+FVector(0,0,P->IsSeated()?35:20),ECC_Visibility,Q);
            O->SetBoolField(TEXT("subject_unobstructed"),!Hidden);O->SetNumberField(TEXT("camera_distance_cm"),FVector::Dist(Camera->GetActorLocation(),P->GetActorLocation()));
        }
    }
    return EWSocial::Encode(O);
}
bool AEWPhotoMode::PhotoAction(const FString& Action)
{
#if WITH_EDITOR
    if(GetWorld()->WorldType!=EWorldType::PIE)return false;
    if(Action==TEXT("open"))Open();else if(Action==TEXT("shoot"))Shoot();else if(Action==TEXT("portrait"))TogglePortrait();
    else if(Action==TEXT("selfie"))ToggleSelfie();else if(Action==TEXT("names"))ToggleNames();else if(Action==TEXT("timer"))CycleTimer();
    else if(Action==TEXT("left"))Turn(-20);else if(Action==TEXT("right"))Turn(20);
    else if(Action==TEXT("close")){if(auto* G=GetGameInstance<UEWGameInstance>())G->SetMenu(EEWMenu::None);Close();}
    else if(Action!=TEXT("inspect"))return false;
    if(auto* G=GetGameInstance<UEWGameInstance>())G->RefreshUI();return true;
#else
    return false;
#endif
}
