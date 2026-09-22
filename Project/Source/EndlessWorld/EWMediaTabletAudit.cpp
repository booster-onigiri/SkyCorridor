#include "EWMediaScreen.h"
#include "EWMediaTabletView.h"
#include "EWTerminal.h"
#include "EWBrowserSurface.h"
#include "EWGameInstance.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWDayCycle.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"

namespace
{
bool TabletAuditAllowed(const UWorld* W)
{return W && (W->IsPlayInEditor() || FParse::Param(FCommandLine::Get(),TEXT("EWTablet94Audit")));}
}
FString AEWMediaScreen::TabletPreview(const FString& Action)
{
    if(!TabletAuditAllowed(GetWorld()))return TEXT("Isolated verification required");
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!G || !P || !PC || !G->Manager)return TEXT("World not ready");
    if(Action==TEXT("approach") || Action==TEXT("far") || Action==TEXT("below") || Action==TEXT("behind"))
    {
        G->SetMenu(EEWMenu::None);if(P->IsSeated())P->LeaveSeat();PC->SetViewTarget(P);
        FVector Direction=TabletAnchor->GetForwardVector();Direction.Z=0;Direction.Normalize();
        FVector Position=Station+Direction*(Action==TEXT("far")?850:Action==TEXT("behind")?-180:170);
        if(Action==TEXT("below"))Position.Z-=550;
        P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation((TabletAnchor->GetComponentLocation()-(Position+FVector(0,0,60))).Rotation());
    }
    else if(Action==TEXT("open"))G->Interact();
    else if(Action==TEXT("open_direct"))OpenControls();
    else if(Action==TEXT("close"))G->SetMenu(EEWMenu::None);
    else if(Action==TEXT("film")){if(Surface)Surface->TestSyncFilm();}
    else if(Action==TEXT("day")){if(G->DayCycle)G->DayCycle->SetAuditHour(12);}
    else if(Action==TEXT("night")){if(G->DayCycle)G->DayCycle->SetAuditHour(23);}
    else if(Action==TEXT("escape"))
    {
        FKeyEvent Key(EKeys::Escape,FModifierKeysState(),0,false,0,0);
        FSlateApplication::Get().ProcessKeyDownEvent(Key);FSlateApplication::Get().ProcessKeyUpEvent(Key);
    }
    else if(Action==TEXT("type"))for(TCHAR C:FString(TEXT("waterlight")))
        FSlateApplication::Get().ProcessKeyCharEvent(FCharacterEvent(C,FModifierKeysState(),0,false));
    auto O=Evidence();O->SetStringField(TEXT("preview_action"),Action);O->SetNumberField(TEXT("menu"),int32(G->Menu()));
    O->SetStringField(TEXT("player"),P->GetActorLocation().ToString());O->SetBoolField(TEXT("camera_restored"),PC->GetViewTarget()==P);
    O->SetBoolField(TEXT("camera_is_tablet"),PC->GetViewTarget()==TabletCamera);O->SetBoolField(TEXT("grounded"),P->GetCharacterMovement()->IsMovingOnGround());
    O->SetNumberField(TEXT("ready_chunks"),G->Manager->ReadyCount());O->SetBoolField(TEXT("travelling"),G->Manager->IsTravelling());
    if(auto Focus=FSlateApplication::Get().GetKeyboardFocusedWidget())O->SetStringField(TEXT("focus_type"),Focus->GetTypeAsString());
    FString Out;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Out));return Out;
}
bool AEWMediaScreen::TabletTap(float X,float Y)
{
    if(!TabletAuditAllowed(GetWorld()) || !bTabletOpen || X<0 || X>1280 || Y<0 || Y>960)return false;
    auto* G=GetGameInstance<UEWGameInstance>();auto* PC=UGameplayStatics::GetPlayerController(this,0);
    auto* V=G?G->GetGameViewportClient():nullptr;if(!PC || !V || !V->GetGameViewportWidget())return false;
    const FVector WorldPoint=Tablet->GetComponentTransform().TransformPosition(FVector(0,640-X,480-Y));
    FVector2D Pixel;if(!PC->ProjectWorldLocationToScreen(WorldPoint,Pixel))return false;
    const FGeometry& Geometry=V->GetGameViewportWidget()->GetCachedGeometry();const FVector2D Desktop=Geometry.LocalToAbsolute(Pixel/Geometry.Scale);
    auto& App=FSlateApplication::Get();const FVector2D Old=App.GetCursorPos();App.SetCursorPos(Desktop);
    FPointerEvent Move(0,Desktop,Old,TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState());App.ProcessMouseMoveEvent(Move,false);
    FPointerEvent Down(0,Desktop,Desktop,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
    FPointerEvent Up(0,Desktop,Desktop,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState());
    App.ProcessMouseButtonDownEvent(nullptr,Down);App.ProcessMouseButtonUpEvent(Up);return true;
}
void AEWMediaScreen::CaptureTablet(const FString& Path) const
{
    if(!TabletAuditAllowed(GetWorld()))return;
    if(auto* RT=Tablet->GetRenderTarget())if(auto* Resource=RT->GameThread_GetRenderTargetResource())
    {TArray<FColor> Pixels;if(Resource->ReadPixels(Pixels)){TArray<uint8> PNG;FImageUtils::CompressImageArray(RT->SizeX,RT->SizeY,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*Path);}}
}
