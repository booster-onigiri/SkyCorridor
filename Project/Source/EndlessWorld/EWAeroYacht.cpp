#include "EWAeroYacht.h"
#include "EWLocalization.h"
#include "EWAeroYachtPlan.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWDayCycle.h"
#include "EWSocialSession.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

AEWAeroYacht::AEWAeroYacht()
{
    // During reimport, keep the authored material graphs out of the startup
    // root set. Normal editor, cooking and game launches retain hard references.
    if(IsRunningCommandlet() && FParse::Param(FCommandLine::Get(),TEXT("EWAero87Import")))return;
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AirYachtRoot")));
    for(const TCHAR* Part:{TEXT("Hull"),TEXT("Glass"),TEXT("Deck"),TEXT("Interior"),TEXT("Furniture"),TEXT("Garden"),TEXT("Water"),TEXT("Fins")})
    {
        const FString Name=FString(TEXT("Aero87"))+Part;
        auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*Name);C->SetupAttachment(RootComponent);
        ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(*FString::Printf(TEXT("/Game/EndlessWorld/Kit/SM_%s.SM_%s"),*Name,*Name));
        C->SetStaticMesh(Asset.Object);C->SetMobility(EComponentMobility::Movable);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCanEverAffectNavigation(false);C->SetCastShadow(Name!=TEXT("Aero87Glass") && Name!=TEXT("Aero87Water"));Meshes.Add(C);
    }
    Collision=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WalkableHull"));Collision->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Solid(TEXT("/Game/EndlessWorld/Kit/SM_Aero87Collision.SM_Aero87Collision"));
    Collision->SetStaticMesh(Solid.Object);Collision->SetVisibility(false);Collision->SetHiddenInGame(true);Collision->SetCastShadow(false);
    Collision->SetMobility(EComponentMobility::Movable);Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Block);Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Collision->SetCanEverAffectNavigation(false);Collision->SetGenerateOverlapEvents(false);Collision->ComponentTags.Add(TEXT("EWFloor"));
    Door=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardingDoor"));Door->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Game/EndlessWorld/Kit/SM_Wall.SM_Wall"));Door->SetStaticMesh(Cube.Object);
    Door->SetRelativeScale3D(FVector(3.6,.12,3.35));Door->SetMobility(EComponentMobility::Movable);Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Door->bDisallowNanite=true; // The sliding door uses translucent glazing.
    DoorBody=CreateDefaultSubobject<UBoxComponent>(TEXT("BoardingDoorCollision"));DoorBody->SetupAttachment(RootComponent);
    DoorBody->SetBoxExtent(FVector(180,14,170));DoorBody->SetRelativeLocation(EWAeroYachtPlan::Door()+FVector(0,0,170));
    DoorBody->SetCollisionResponseToAllChannels(ECR_Block);DoorBody->SetCollisionObjectType(ECC_WorldDynamic);DoorBody->SetCanEverAffectNavigation(false);
    for(int I=0;I<12;++I)
    {
        auto* L=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("YachtLight%d"),I));L->SetupAttachment(RootComponent);
        const bool Deck=I>=10;const double Xs[]={-4000,-2000,0,2000,3900};
        const double X=Deck?(I==10?-1800:3100):Xs[I/2];
        // Emit below each actual pendant, clear of the lowered pool coffer.
        const double Z=Deck?900:((X>-2500 && X<3000)?289:437);
        L->SetRelativeLocation(FVector(X,Deck?0:(I%2?400:-400),Z));
        L->SetMobility(EComponentMobility::Movable);L->SetIntensityUnits(ELightUnits::Lumens);L->SetIntensity(0);
        L->SetAttenuationRadius(Deck?1600:1900);L->SetSourceRadius(35);L->SetLightColor(Deck?FLinearColor(.55,.76,1):FLinearColor(1,.76,.49));
        L->SetCastShadows(!Deck && (I==0 || I==3 || I==6 || I==9));L->SetVolumetricScatteringIntensity(0);L->SetIndirectLightingIntensity(.35);Lights.Add(L);
    }
    for(int I=0;I<8;++I)
    {
        const double Xs[]={-400,500,1400,2900};const double X=Xs[I/2];
        auto* L=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("PergolaLamp%d"),I));L->SetupAttachment(RootComponent);
        L->SetRelativeLocation(FVector(X,(I%2?1:-1)*(X<2500?1140:900),877));
        L->SetMobility(EComponentMobility::Movable);L->SetIntensityUnits(ELightUnits::Lumens);L->SetIntensity(0);
        L->SetAttenuationRadius(1000);L->SetSourceRadius(18);L->SetLightColor(FLinearColor(1,.67,.34));
        L->SetCastShadows(false);L->SetVolumetricScatteringIntensity(0);L->SetIndirectLightingIntensity(.2);DeckLamps.Add(L);
    }
}
void AEWAeroYacht::BeginPlay()
{
    Super::BeginPlay();SetActorHiddenInGame(true);
    if(auto* G=GetGameInstance<UEWGameInstance>())if(G->Manager)AddTickPrerequisiteActor(G->Manager);
    if(auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))P->GetCharacterMovement()->AddTickPrerequisiteActor(this);
    if(auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_Aero87Glow.M_Aero87Glow")))
    {
        Glow=UMaterialInstanceDynamic::Create(M,this);
        for(auto C:Meshes)for(int I=0;I<C->GetNumMaterials();++I)
            if(C->GetMaterial(I) && C->GetMaterial(I)->GetName().Contains(TEXT("Aero87Glow")))C->SetMaterial(I,Glow);
    }
    Door->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_Aero87Glass.M_Aero87Glass")));
}
double AEWAeroYacht::Clock() const
{
    if(OverrideClock>=0)return OverrideClock;
    const auto* G=GetGameInstance<UEWGameInstance>();
    // 300 seconds divides the 1800-second day, so midnight never jumps the
    // route. Online peers derive the same two phases from the shared city clock.
    return G && G->SocialSession && G->SocialSession->HasSharedHour()?G->SocialSession->SharedHour()*75.:GetWorld()->GetTimeSeconds();
}
bool AEWAeroYacht::AtDock() const{return bReady && EWAeroYachtPlan::Docked(Clock(),Index);}
bool AEWAeroYacht::CanBoard() const{return bReady && EWAeroYachtPlan::GangwayOpen(Clock(),Index);}
FString AEWAeroYacht::Name() const{return Index?EWL::Pick(TEXT("月凪 / TSUKINAGI"), TEXT("TSUKINAGI / Moon Calm")):EWL::Pick(TEXT("白鷺 / SHIRASAGI"), TEXT("SHIRASAGI / White Heron"));}
bool AEWAeroYacht::Contains(const AEWCharacter* P) const
{
    if(!bReady || !P)return false;
    const FVector V=GetActorTransform().InverseTransformPosition(P->GetActorLocation());
    const double Half=2165*FMath::Sqrt(FMath::Max(0.,1-FMath::Square(V.X/5700.)));
    return FMath::Abs(V.X)<5675 && FMath::Abs(V.Y)<Half+50 && V.Z>-120 && V.Z<2050;
}
FString AEWAeroYacht::Hint(const AEWCharacter* P) const
{
    if(!Contains(P))return {};
    return Name()+(AtDock()?EWL::Format(TEXT("　寄港中・出港まで %d 秒　乗り口から下船できます"), TEXT("  Docked · Departure in %d s · Disembark through the boarding door"),FMath::Max(0,FMath::CeilToInt(EWAeroYachtPlan::Dwell-EWAeroYachtPlan::Phase(Clock(),Index)))):
        EWL::Pick(TEXT("　都市遊覧中　船内を自由に歩けます / 階段から上部プールデッキへ"), TEXT("  Cruising over the city · Explore the cabin / Take the stairs to the pool deck")));
}
void AEWAeroYacht::Tick(float Delta)
{
    if(bCinematicClock && !bApplyingCinematicClock)return;
    if(OverrideClock>=0 && bAdvanceOverride)OverrideClock+=Delta;
    Super::Tick(Delta);const auto* G=GetGameInstance<UEWGameInstance>();const auto* M=G?G->Manager.Get():nullptr;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    const bool Enabled=M && M->bHasWorld && M->Descriptor().Seed==EW::WorldDescriptor::ReferenceWorld().Seed;
    bReady=Enabled && Collision->GetStaticMesh() && Collision->GetStaticMesh()->GetBodySetup() && Collision->GetStaticMesh()->GetBodySetup()->AggGeom.ConvexElems.Num()>500;
    SetActorHiddenInGame(!bReady);if(!bReady){Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);DoorBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);return;}
    FTransform T=EWAeroYachtPlan::Pose(Clock(),Index);T.AddToTranslation(M->ToRender({0,0},FVector::ZeroVector));
    const FVector Before=GetActorLocation();
    SetActorTransform(T,false,nullptr,ETeleportType::None);
    const FVector Velocity=Delta>SMALL_NUMBER?(T.GetLocation()-Before)/Delta:FVector::ZeroVector;
    RootComponent->ComponentVelocity=Velocity;Collision->ComponentVelocity=Velocity;
    const bool Near=P && FVector::DistSquared(P->GetActorLocation(),T.GetLocation())<FMath::Square(13000.);
    Collision->SetCollisionEnabled(Near?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    const bool Open=CanBoard();
    // Clear the threshold before doors close, so a passenger can never be
    // trapped between a moving hull and the fixed boarding bridge.
    if(bWasOpen && !Open && P && !M->IsTravelling())
    {
        const FVector L=T.InverseTransformPosition(P->GetActorLocation());
        if(FMath::Abs(L.X+2400)<230 && L.Y>-2150 && L.Y<-1720 && L.Z<260 && L.Z>-50)
        {P->SetActorLocation(T.TransformPosition(FVector(-2400,-1680,99)),false,nullptr,ETeleportType::TeleportPhysics);P->ResetSafeLocation();}
    }
    bWasOpen=Open;Door->SetRelativeLocation(EWAeroYachtPlan::Door()+FVector(Open?-380:0,0,170));
    DoorBody->SetCollisionEnabled(Near && !Open?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    const float Scale=G->DayCycle?G->DayCycle->InteriorLightScale():1;
    const float Evening=G->DayCycle?G->DayCycle->StreetLightScale():0;
    const float Night=Scale>SMALL_NUMBER?Evening/Scale:0;
    if(Glow)Glow->SetScalarParameterValue(TEXT("Power"),FMath::Lerp(Index?1000.f:1250.f,4200.f,Night)*Scale);
    for(int I=0;I<Lights.Num();++I){Lights[I]->SetVisibility(Near);Lights[I]->SetIntensity(Near?Scale*(I<10?FMath::Lerp(450000.f,700000.f,Night):FMath::Lerp(160000.f,320000.f,Night)):0);}
    for(auto L:DeckLamps){L->SetVisibility(Near && Night>.01);L->SetIntensity(Near?Evening*600000:0);}
    UpdatePreview();
}
TSharedRef<FJsonObject> AEWAeroYacht::Evidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("ready"),bReady);O->SetNumberField(TEXT("index"),Index);O->SetStringField(TEXT("name"),Name());
    O->SetNumberField(TEXT("phase_seconds"),EWAeroYachtPlan::Phase(Clock(),Index));O->SetNumberField(TEXT("period"),EWAeroYachtPlan::Period);
    O->SetBoolField(TEXT("docked"),AtDock());O->SetBoolField(TEXT("boarding_open"),CanBoard());O->SetStringField(TEXT("position"),GetActorLocation().ToString());
    O->SetNumberField(TEXT("floor_z"),EWAeroYachtPlan::Design().Ship.Z);O->SetNumberField(TEXT("minimum_roof_clearance_cm"),EWAeroYachtPlan::Design().Ship.Z-1400-EWAeroYachtPlan::Design().Ceiling);
    O->SetNumberField(TEXT("convex_bodies"),Collision->GetStaticMesh() && Collision->GetStaticMesh()->GetBodySetup()?Collision->GetStaticMesh()->GetBodySetup()->AggGeom.ConvexElems.Num():0);
    O->SetBoolField(TEXT("collision_enabled"),Collision->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
    int32 Loaded=0;for(const auto C:Meshes)if(C->GetStaticMesh())++Loaded;O->SetNumberField(TEXT("loaded_meshes"),Loaded);
    int32 Active=0;for(const auto L:DeckLamps)if(L->IsVisible() && L->Intensity>0)++Active;
    O->SetNumberField(TEXT("evening_deck_lamps"),Active);O->SetNumberField(TEXT("light_component_budget"),Lights.Num()+DeckLamps.Num());
    return O;
}
FString AEWAeroYacht::YachtEvidence() const{FString T;FJsonSerializer::Serialize(Evidence(),TJsonWriterFactory<>::Create(&T));return T;}
bool AEWAeroYacht::SetCinematicClock(double Seconds)
{
    const auto* G=GetGameInstance<UEWGameInstance>();FString Data,User;
    if(!G || !G->bScriptedWorldAudit || !FParse::Param(FCommandLine::Get(),TEXT("EWCinematic96")) ||
       !FParse::Param(FCommandLine::Get(),TEXT("EWCinematic95")) ||
       !FParse::Value(FCommandLine::Get(),TEXT("EWDataDir="),Data) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),User) ||
       FPaths::IsRelative(Data) || FPaths::IsRelative(User) ||
       !FMath::IsFinite(Seconds) || Seconds<0 || Seconds>632)return false;
    if(!bCinematicClock){PreviousCinematicClock=OverrideClock;bPreviousAdvance=bAdvanceOverride;}
    bCinematicClock=true;bApplyingCinematicClock=true;bAdvanceOverride=false;OverrideClock=Seconds;
    // Reuse the shipped cruise pose, deck collision, doors and practical lights.
    Tick(0);bApplyingCinematicClock=false;return bReady;
}
bool AEWAeroYacht::CinematicDeckFrame(FTransform& Out) const
{if(!bCinematicClock || !bReady)return false;Out=GetActorTransform();return true;}
void AEWAeroYacht::ReleaseCinematicClock()
{
    if(bCinematicClock){OverrideClock=PreviousCinematicClock;bAdvanceOverride=bPreviousAdvance;}
    bCinematicClock=false;bApplyingCinematicClock=false;
}
bool AEWAeroYacht::AuditTime(double Seconds)
{
    if(!GIsEditor && !FParse::Param(FCommandLine::Get(),TEXT("EWAero87Audit")))return false;
    if(!FMath::IsFinite(Seconds) || Seconds<0)return false;OverrideClock=Seconds;Tick(0);return true;
}
bool AEWAeroYacht::PreviewYacht(int32 View,double Hour)
{
#if WITH_EDITOR
    if(!GIsEditor || !bReady || View<0 || View>4 || Hour<0 || Hour>=24)return false;
    auto* G=GetGameInstance<UEWGameInstance>();if(G && G->DayCycle)G->DayCycle->SetAuditHour(Hour);
    if(!PreviewCamera)PreviewCamera=GetWorld()->SpawnActor<ACameraActor>();PreviewView=View;UpdatePreview();
    // Preview the same streamed neighbourhood and practical lighting a real
    // passenger sees, instead of leaving the player behind at the old port.
    if(auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))
    {
        P->LeaveSeat();P->ClearHeldInput();P->SetStreamingHold(false);
        Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        P->SetBase(static_cast<UPrimitiveComponent*>(nullptr));
        P->SetActorLocation(GetActorTransform().TransformPosition(View<=1?FVector(3500,0,699):FVector(-2100,-600,99)),false,nullptr,ETeleportType::TeleportPhysics);
        P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);P->GetCharacterMovement()->bForceNextFloorCheck=true;P->ResetSafeLocation();
    }
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(PreviewCamera);return true;
#else
    return false;
#endif
}
void AEWAeroYacht::UpdatePreview()
{
    if(!PreviewCamera || PreviewView<0)return;
    const FVector Eyes[]={FVector(11200,-10300,6900),FVector(3300,-1050,780),FVector(-2100,-600,165),FVector(-3850,-290,165),FVector(-3100,-1750,190)};
    const FVector Targets[]={FVector(300,0,180),FVector(-1600,0,670),FVector(3200,0,170),FVector(-4000,-630,170),FVector(-1300,-1330,650)};
    const auto T=GetActorTransform();const FVector Eye=T.TransformPosition(Eyes[PreviewView]),Target=T.TransformPosition(Targets[PreviewView]);
    PreviewCamera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation());PreviewCamera->GetCameraComponent()->SetFieldOfView(PreviewView==0?63:78);
}

AEWAeroPort::AEWAeroPort()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SkyportRoot")));
    BridgePivot=CreateDefaultSubobject<USceneComponent>(TEXT("BridgeHinge"));BridgePivot->SetupAttachment(RootComponent);BridgePivot->SetRelativeLocation(FVector(0,EWAeroYachtPlan::GangwayStart,0));
    Bridge=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RetractableGangway"));Bridge->SetupAttachment(BridgePivot);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Game/EndlessWorld/Kit/SM_Wall.SM_Wall"));Bridge->SetStaticMesh(Cube.Object);
    Bridge->SetRelativeLocation(FVector(0,200,-12));Bridge->SetRelativeScale3D(FVector(3.5,4,.24));Bridge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BridgeBody=CreateDefaultSubobject<UBoxComponent>(TEXT("GangwayFloor"));BridgeBody->SetupAttachment(BridgePivot);
    BridgeBody->SetRelativeLocation(FVector(0,200,-12));BridgeBody->SetBoxExtent(FVector(175,200,12));BridgeBody->ComponentTags.Add(TEXT("EWFloor"));
    BridgeBody->SetCollisionResponseToAllChannels(ECR_Block);BridgeBody->SetCollisionObjectType(ECC_WorldDynamic);BridgeBody->SetCanEverAffectNavigation(false);
    for(int Side=0;Side<2;++Side)
    {
        const float X=Side?170:-170;
        auto* Rail=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("GangwayRail%d"),Side));Rail->SetupAttachment(BridgePivot);
        static ConstructorHelpers::FObjectFinder<UStaticMesh> Railing(TEXT("/Game/EndlessWorld/Kit/SM_StoneRail.SM_StoneRail"));Rail->SetStaticMesh(Railing.Object);
        Rail->SetRelativeLocation(FVector(X,200,0));Rail->SetRelativeRotation(FRotator(0,90,0));Rail->SetRelativeScale3D(FVector(1,.55,1));Rail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        auto* Body=CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("GangwayRailCollision%d"),Side));Body->SetupAttachment(BridgePivot);
        Body->SetRelativeLocation(FVector(X,200,56));Body->SetBoxExtent(FVector(6,200,56));Body->SetCollisionResponseToAllChannels(ECR_Block);Body->SetCanEverAffectNavigation(false);BridgeRailBodies.Add(Body);
    }
    Gate=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PierSafetyGate"));Gate->SetupAttachment(RootComponent);Gate->SetStaticMesh(Cube.Object);
    Gate->SetRelativeScale3D(FVector(3.35,.15,1.1));Gate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GateBody=CreateDefaultSubobject<UBoxComponent>(TEXT("PierSafetyCollision"));GateBody->SetupAttachment(RootComponent);
    GateBody->SetRelativeLocation(FVector(0,EWAeroYachtPlan::GangwayStart-20,70));GateBody->SetBoxExtent(FVector(175,10,70));GateBody->SetCollisionResponseToAllChannels(ECR_Block);GateBody->SetCanEverAffectNavigation(false);
    Sign=CreateDefaultSubobject<UWidgetComponent>(TEXT("DepartureBoard"));Sign->SetupAttachment(RootComponent);
    Sign->SetWidgetSpace(EWidgetSpace::World);Sign->SetDrawSize(FVector2D(1600,420));Sign->SetPivot(FVector2D(.5,.5));Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sign->SetRelativeLocation(FVector(-1000,1400,270));Sign->SetRelativeRotation(FRotator(0,-90,0));Sign->SetRelativeScale3D(FVector(.40));Sign->SetTwoSided(true);
}
void AEWAeroPort::BeginPlay()
{
    Super::BeginPlay();SetActorHiddenInGame(true);
    if(auto* G=GetGameInstance<UEWGameInstance>())if(G->Manager)AddTickPrerequisiteActor(G->Manager);
    Bridge->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_Timber.M_Timber")));
    Gate->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_InteriorBrass.M_InteriorBrass")));
    Sign->SetSlateWidget(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.005,.017,.022,1)).Padding(24)
        [SNew(STextBlock).Font(FSlateFontInfo(FPaths::ProjectContentDir()/TEXT("Fonts/DroidSansFallback.ttf"),44)).Justification(ETextJustify::Center)
        .ColorAndOpacity(FLinearColor(.8,.89,.85)).Text_Lambda([this]{return FText::FromString(Status());})]);
}
FString AEWAeroPort::Status() const
{
    const auto* G=GetGameInstance<UEWGameInstance>();if(!G)return {};
    double Wait=EWAeroYachtPlan::Period;FString Next;
    for(const auto Y:G->Yachts)if(Y && Y->Ready())
    {
        const double Phase=EWAeroYachtPlan::Phase(Y->Clock(),Y->Index);
        if(Y->AtDock())return TEXT("AURELIA SKYPORT\n")+Y->Name()+
            (Y->CanBoard()?EWL::Pick(TEXT("　乗船できます"), TEXT("  Boarding open")):EWL::Pick(TEXT("　乗船橋を操作中"), TEXT("  Boarding bridge moving")))+
            EWL::Format(TEXT("\n出港まで %d 秒　橋を渡って船内へ"), TEXT("\nDeparture in %d s · Cross the bridge to board"),FMath::Max(0,FMath::CeilToInt(EWAeroYachtPlan::Dwell-Phase)));
        if(EWAeroYachtPlan::Period-Phase<Wait){Wait=EWAeroYachtPlan::Period-Phase;Next=Y->Name();}
    }
    return EWL::Pick(TEXT("AURELIA SKYPORT\n次の遊覧船："), TEXT("AURELIA SKYPORT\nNext sky yacht: "))+Next+EWL::Format(TEXT("\n到着まで 約 %d 秒　ゲートの手前でお待ちください"), TEXT("\nArrival in about %d s · Please wait before the gate"),FMath::CeilToInt(Wait));
}
void AEWAeroPort::Tick(float Delta)
{
    Super::Tick(Delta);const auto* G=GetGameInstance<UEWGameInstance>();const auto* M=G?G->Manager.Get():nullptr;
    bReady=M && M->Descriptor().Seed==EW::WorldDescriptor::ReferenceWorld().Seed && M->ReadyAt({1,0});
    SetActorHiddenInGame(!bReady);bool Open=false;
    if(bReady)
    {
        SetActorLocation(M->ToRender({0,0},EWAeroYachtPlan::Design().Terminal));
        for(const auto Y:G->Yachts)if(Y && Y->CanBoard())Open=true;
    }
    if(bOpen && !Open)
    {
        if(auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))
        {
            const FVector L=GetActorTransform().InverseTransformPosition(P->GetActorLocation());
            if(FMath::Abs(L.X)<215 && L.Y>EWAeroYachtPlan::GangwayStart-120 && L.Y<EWAeroYachtPlan::GangwayStart+260 && L.Z<260 && L.Z>-50)
            {P->SetActorLocation(GetActorTransform().TransformPosition(FVector(0,EWAeroYachtPlan::GangwayStart-240,99)),false,nullptr,ETeleportType::TeleportPhysics);P->ResetSafeLocation();}
        }
    }
    bOpen=Open;Extension=FMath::FInterpConstantTo(Extension,Open?1.f:0.f,Delta,1.5f);
    BridgePivot->SetRelativeRotation(FRotator(0,0,-82*(1-Extension)));
    Gate->SetRelativeLocation(FVector(0,EWAeroYachtPlan::GangwayStart-20,70-180*Extension));
    BridgeBody->SetCollisionEnabled(bReady && Open && Extension>.98?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    for(auto Body:BridgeRailBodies)Body->SetCollisionEnabled(BridgeBody->GetCollisionEnabled());
    GateBody->SetCollisionEnabled(bReady && (!Open || Extension<.98)?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    const float Scale=G && G->DayCycle?G->DayCycle->InteriorLightScale():1;Sign->SetTintColorAndOpacity(FLinearColor(6000*Scale,6000*Scale,6000*Scale,1));
}
TSharedRef<FJsonObject> AEWAeroPort::Evidence() const
{auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("ready"),bReady);O->SetBoolField(TEXT("open"),bOpen);O->SetNumberField(TEXT("extension"),Extension);O->SetBoolField(TEXT("gate_blocks"),GateBody->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);return O;}
