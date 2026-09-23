#include "EWSkyrail.h"
#include "EWLocalization.h"
#include "EWSkyrailPlan.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWGameInstance.h"
#include "EWDayCycle.h"
#include "EWHotelPlan.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

AEWSkyrail::AEWSkyrail()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RailRoot")));
    Train=CreateDefaultSubobject<USceneComponent>(TEXT("MovingTrain"));Train->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Body(TEXT("/Game/EndlessWorld/Kit/SM_SkyrailCar.SM_SkyrailCar"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Glass(TEXT("/Game/EndlessWorld/Kit/SM_SkyrailGlass.SM_SkyrailGlass"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Door(TEXT("/Game/EndlessWorld/Kit/SM_SkyrailDoor.SM_SkyrailDoor"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cab(TEXT("/Game/EndlessWorld/Kit/SM_Skyrail82Cab.SM_Skyrail82Cab"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CabGlass(TEXT("/Game/EndlessWorld/Kit/SM_Skyrail82CabGlass.SM_Skyrail82CabGlass"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Trim(TEXT("/Game/EndlessWorld/Kit/SM_Skyrail82Trim.SM_Skyrail82Trim"));
    auto Solid=[this](USceneComponent* Parent,const FString& Name,FVector P,FVector E,bool Floor=false)
    {
        auto* C=CreateDefaultSubobject<UBoxComponent>(*Name);C->SetupAttachment(Parent);C->SetRelativeLocation(P);C->SetBoxExtent(E);
        C->SetMobility(EComponentMobility::Movable);C->SetCollisionObjectType(ECC_WorldDynamic);C->SetCollisionResponseToAllChannels(ECR_Block);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);C->SetGenerateOverlapEvents(false);
        C->ComponentTags.Add(Floor?TEXT("EWFloor"):TEXT("EWObstacle"));Collision.Add(C);
    };
    for(int32 I=0;I<2;++I)
    {
        auto* Frame=CreateDefaultSubobject<USceneComponent>(*FString::Printf(TEXT("CarFrame%d"),I));Frame->SetupAttachment(Train);CarFrames.Add(Frame);
        const double Centre=0;
        for(int Layer=0;Layer<3;++Layer)
        {
            auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Streamline%d_%d"),I,Layer));
            C->SetupAttachment(Frame);C->SetStaticMesh(Layer==0?Cab.Object:Layer==1?CabGlass.Object:Trim.Object);
            C->SetRelativeLocation(FVector(Layer<2?(I?524:-524):Centre,0,0));
            if(Layer<2)C->SetRelativeRotation(FRotator(0,I?0:180,0));
            C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(Layer!=1);BodyDetails.Add(C);
        }
        Solid(Frame,FString::Printf(TEXT("CarFloor%d"),I),FVector(Centre,0,-3),FVector(525,139,8),true);
        Solid(Frame,FString::Printf(TEXT("CarRoof%d"),I),FVector(Centre,0,306),FVector(536,149,14),true);
        Solid(Frame,FString::Printf(TEXT("CarSouth%d"),I),FVector(Centre,-145,146),FVector(520,6,146));
        for(int Side=0;Side<2;++Side)
        {
            Solid(Frame,FString::Printf(TEXT("CarNorth%d_%d"),I,Side),FVector(Centre+(Side?293:-293),145,146),FVector(227,6,146));
            Solid(Frame,FString::Printf(TEXT("CarEnd%d_%d"),I,Side),FVector(Centre+(Side?519:-519),0,146),FVector(6,142,146));
        }
        Solid(Frame,FString::Printf(TEXT("CabNose%d"),I),FVector(I?575:-575,0,143),FVector(52,115,122));
        for(int32 Layer=0;Layer<2;++Layer)
        {
            auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Car%dLayer%d"),I,Layer));
            C->SetupAttachment(Frame);C->SetMobility(EComponentMobility::Movable);C->SetRelativeLocation(FVector::ZeroVector);
            C->SetStaticMesh(Layer?Glass.Object:Body.Object);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            C->SetCanEverAffectNavigation(false);C->SetCastShadow(Layer==0);Cars.Add(C);
        }
        for(int32 Side=0;Side<2;++Side)
        {
            auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Door%d_%d"),I,Side));
            C->SetupAttachment(Frame);C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(Door.Object);
            C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCanEverAffectNavigation(false);Doors.Add(C);
            Solid(C,FString::Printf(TEXT("DoorSolid%d_%d"),I,Side),FVector(0,146,142),FVector(31,5,142));
        }
    }
    for(int32 I=0;I<2+EWSkyrailPlan::Stations;++I)
    {
        auto* L=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("RailLight%d"),I));
        L->SetupAttachment(I<2?CarFrames[I].Get():RootComponent.Get());L->SetMobility(EComponentMobility::Movable);
        L->SetIntensityUnits(ELightUnits::Lumens);L->SetIntensity(0);L->SetAttenuationRadius(I<2?580:900);
        L->SetLightColor(FLinearColor(1,.76,.49));L->SetSourceRadius(20);L->SetCastShadows(true);
        L->SetVolumetricScatteringIntensity(0);L->SetIndirectLightingIntensity(.4);Lights.Add(L);
        if(I<2)L->SetRelativeLocation(FVector(0,0,275));
    }
    for(int32 I=0;I<EWSkyrailPlan::Stations;++I)
    {
        auto* S=CreateDefaultSubobject<UWidgetComponent>(*FString::Printf(TEXT("StationSign%d"),I));
        S->SetupAttachment(RootComponent);S->SetWidgetSpace(EWidgetSpace::World);S->SetDrawSize(FVector2D(1200,260));
        S->SetPivot(FVector2D(.5,.5));S->SetCollisionEnabled(ECollisionEnabled::NoCollision);S->SetTwoSided(true);
        S->SetRelativeScale3D(FVector(.4));S->SetRelativeRotation(FRotator(0,-90,0));S->SetTickWhenOffscreen(false);Signs.Add(S);
    }
}
void AEWSkyrail::BeginPlay()
{
    Super::BeginPlay();SetActorHiddenInGame(true);
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_NightFixtureGlow.M_NightFixtureGlow"));
    if(Material)Glow=UMaterialInstanceDynamic::Create(Material,this);
    for(const auto& C:Cars)if(C && C->GetStaticMesh())for(int32 I=0;I<C->GetNumMaterials();++I)
        if(Glow && C->GetMaterial(I) && C->GetMaterial(I)->GetName().Contains(TEXT("Glow")))C->SetMaterial(I,Glow);
    for(const auto& C:BodyDetails)for(int I=0;I<C->GetNumMaterials();++I)
        if(Glow && C->GetMaterial(I) && C->GetMaterial(I)->GetName().Contains(TEXT("Glow")))C->SetMaterial(I,Glow);
    for(int32 I=0;I<Signs.Num();++I)Signs[I]->SetSlateWidget(
        SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.008,.03,.032,1)).Padding(20)
        [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",32)).AutoWrapText(true).ColorAndOpacity(FLinearColor(.84,.92,.81))
        .Text_Lambda([this,I]{return FText::FromString(StationText(I));})]);
}
FVector AEWSkyrail::BoardingPosition(int32 I) const {return RenderOffset+EWSkyrailPlan::Boarding(I,Line);}
int32 AEWSkyrail::AtStation() const {return Phase%2==0?EWSkyrailPlan::Departure(Phase,Line):INDEX_NONE;}
int32 AEWSkyrail::Destination() const {return EWSkyrailPlan::Destination(Phase,Line);}
FTransform AEWSkyrail::StandAt(int32 I) const {return FTransform(FRotator(0,270,0),BoardingPosition(I)+FVector(RiderCar*1120,0,0));}
FVector AEWSkyrail::LiftEntry(int32 I) const
{
    const auto* G=GetGameInstance<UEWGameInstance>();const auto C=EWSkyrailPlan::StopChunk(I,Line);
    const auto R=G&&G->Manager?G->Manager->RecipeAt(C):nullptr;
    if(R)for(const auto& L:R->Lifts)if(L.Id==EWSkyrailPlan::LiftId(I,Line))for(const auto& S:L.Stops)
        if(!Line || FMath::Abs(S.Height-EWSkyrailPlan::Height(Line))<1)return G->Manager->ToRender(C,S.Landing)+FVector(0,0,91);
    return BoardingPosition(I);
}
bool AEWSkyrail::IsRider(const AEWCharacter* P) const {return P && Rider.Get()==P && P->IsSeated();}
int32 AEWSkyrail::NearbyStation(const AEWCharacter* P) const
{
    if(!bReady || !P || P->IsLiftRiding())return INDEX_NONE;
    for(int32 I=0;I<EWSkyrailPlan::Count(Line);++I)for(int32 Car=0;Car<2;++Car)
        if(FVector::DistSquared(P->GetActorLocation(),BoardingPosition(I)+FVector(Car*1120,0,0))<FMath::Square(260.))return I;
    return INDEX_NONE;
}
FString AEWSkyrail::StationText(int32 I) const
{
    if(I>=EWSkyrailPlan::Count(Line))return {};
    const bool Here=AtStation()==I;
    double Seconds=EWSkyrailPlan::Duration(Phase,Line)-Elapsed;
    for(int32 Next=(Phase+1)%EWSkyrailPlan::PhaseCount(Line),Count=0;!Here && Count<EWSkyrailPlan::PhaseCount(Line);++Count,Next=(Next+1)%EWSkyrailPlan::PhaseCount(Line))
    {if(Next%2==0 && EWSkyrailPlan::Departure(Next,Line)==I)break;Seconds+=EWSkyrailPlan::Duration(Next,Line);}
    const int32 Wait=FMath::CeilToInt(Seconds);
    const FString Guide=Line?(I==0?EWL::Pick(TEXT("101–108号室 → 右の昇降機で客室階へ"), TEXT("Rooms 101–108 → Elevator on the right to guest rooms")):I==1?EWL::Pick(TEXT("右の昇降機：上へ 天空シアター / 下へ 時計広場・水都線"), TEXT("Right elevator: up to Sky Theatre / down to Clock Plaza and Water City Line")):EWL::Pick(TEXT("右奥の昇降機 → 最上階・豪華飛行船の搭乗口"), TEXT("Elevator at the rear right → Rooftop and sky yacht boarding"))):
        (I==0?EWL::Pick(TEXT("右の昇降機 → 上層線（ホテル・シアター・空港）"), TEXT("Right elevator → Upper Line (Hotel · Theatre · Skyport)")):EWL::Pick(TEXT("右の昇降機 → 水鏡の聖堂・水辺の広場"), TEXT("Right elevator → Mirrorwater Sanctuary and Waterfront Plaza")));
    return EWL::Translate(EWSkyrailPlan::Name(I,Line))+EWL::Pick(TEXT("駅　"), TEXT(" Station  "))+EWL::Translate(EWSkyrailPlan::LineName(Line))+TEXT("\n")+
        (Here?EWL::Format(TEXT("次は %s　発車まで %d 秒"), TEXT("Next: %s · Departure in %d s"),*EWL::Translate(EWSkyrailPlan::Name(Destination(),Line)),FMath::Max(0,Wait)):
              EWL::Format(TEXT("次の電車まで %d 秒"), TEXT("Next train in %d s"),FMath::Max(0,Wait)))+TEXT("\n")+Guide;
}
FString AEWSkyrail::Hint(const AEWCharacter* P) const
{
    if(IsRider(P))return AtStation()!=INDEX_NONE && DoorOpen>.95?
        EWL::Translate(EWSkyrailPlan::Name(AtStation(),Line))+EWL::Pick(TEXT("駅　E / WASD 降りる　マウス 見回す"), TEXT(" Station  E / WASD: leave train · Mouse: look around")):
        EWL::Pick(TEXT("空中電車　次は "), TEXT("Skyrail  Next: "))+EWL::Translate(EWSkyrailPlan::Name(Destination(),Line))+EWL::Pick(TEXT("　マウスで車窓を眺める"), TEXT("  Mouse: enjoy the view"));
    const int32 I=NearbyStation(P);if(I==INDEX_NONE)return {};
    return AtStation()==I && DoorOpen>.95?EWL::Translate(EWSkyrailPlan::Name(I,Line))+EWL::Pick(TEXT("駅　空中電車に乗る　E"), TEXT(" Station  E: board skyrail")):
        EWL::Translate(EWSkyrailPlan::Name(I,Line))+EWL::Pick(TEXT("駅　電車を待つ（発車案内はホームの表示板へ）"), TEXT(" Station  Wait for the train (see the platform departure board)"));
}
bool AEWSkyrail::Interact(AEWCharacter* P)
{
    auto* G=GetGameInstance<UEWGameInstance>();
    if(IsRider(P))
    {
        if(AtStation()!=INDEX_NONE && DoorOpen>.95){P->UpdateTransitSeat(this,P->GetActorLocation(),StandAt(AtStation()),false);if(P->LeaveSeat()){Rider.Reset();++Alightings;}}
        else if(G)G->Notify(EWL::Pick(TEXT("走行中です。駅に停車してから降りられます。"), TEXT("The train is moving. You can leave once it stops at a station.")));
        return true;
    }
    const int32 I=NearbyStation(P);if(I==INDEX_NONE)return false;
    if(AtStation()!=I || DoorOpen<.95){if(G)G->Notify(EWL::Pick(TEXT("電車が到着するまでホームでお待ちください。"), TEXT("Please wait on the platform for the train to arrive.")));return true;}
    RiderCar=P->GetActorLocation().X>RenderOffset.X+EWSkyrailPlan::StopX(I,Line)?1:0;
    if(P->SitAtTransform(this,FTransform(FRotator(0,270,0),CarFrames[RiderCar]->GetComponentTransform().TransformPosition(FVector(-260,82,103))),StandAt(I)))
    {Rider=P;BoardedAt=I;++Boardings;Elapsed=FMath::Min(Elapsed,EWSkyrailPlan::Dwell-6);if(G)G->Notify(EWL::Pick(TEXT("次は "), TEXT("Next: "))+EWL::Translate(EWSkyrailPlan::Name(Destination(),Line))+EWL::Pick(TEXT("。まもなく出発します。"), TEXT(". Departing shortly.")));}
    return true;
}
void AEWSkyrail::ReleaseRider()
{
    if(auto* P=Rider.Get())if(P->IsSeated())
    {P->UpdateTransitSeat(this,P->GetActorLocation(),StandAt(AtStation()==INDEX_NONE?BoardedAt:AtStation()),false);P->LeaveSeat();}
    Rider.Reset();
}
void AEWSkyrail::Tick(float Delta)
{
    if(bCinematicClock && !bApplyingCinematicClock)return;
    Super::Tick(Delta);const auto* G=GetGameInstance<UEWGameInstance>();const auto* M=G?G->Manager.Get():nullptr;
    // A water-city passenger may be four chunks from the old terminus. Keeping
    // both old stations resident is neither necessary nor a valid readiness gate.
    const bool Ready=M && EWSkyrailPlan::Enabled(M->Descriptor()) && (Rider.IsValid() || M->ReadyAt(M->PlayerCoord()));
    if(bReady!=Ready){bReady=Ready;SetActorHiddenInGame(!Ready);for(const auto& C:Collision)C->SetCollisionEnabled(Ready?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);}
    if(!Ready || M->IsTravelling())return;
    RenderOffset=M->ToRender({0,0},FVector::ZeroVector);
    if(!bGuidesBuilt)BuildGuides();
    for(int I=0;I<Guides.Num();++I)
    {const auto P=GuidePositions[I];Guides[I]->SetWorldLocation(RenderOffset+P);Guides[I]->SetVisibility(M->ReadyAt({int64(P.X/EW::ChunkSize),int64(P.Y/EW::ChunkSize)}));}
    const double Step=FMath::Min(double(Delta),.15),Duration=EWSkyrailPlan::Duration(Phase,Line);
    auto TrackX=[&](double T){return Phase%2?FMath::Lerp(EWSkyrailPlan::StopX(EWSkyrailPlan::Departure(Phase,Line),Line),
        EWSkyrailPlan::StopX(Destination(),Line),EWSkyrailPlan::Progress(T,Duration)):EWSkyrailPlan::StopX(AtStation(),Line);};
    const double OldX=TrackX(Elapsed),NextX=TrackX(Elapsed+Step);
    const EW::ChunkCoord NextChunk{int64(NextX/EW::ChunkSize),0};
    // Hold a rider before an unloaded chunk, retaining their seat and camera.
    // The station doors also wait for collision to exist before allowing exit.
    const bool StreamReady=!Rider.IsValid() || M->ReadyAt(NextChunk);
    if(StreamReady)Elapsed+=Step;
    double X=TrackX(Elapsed);
    Speed=Step>0?FMath::Abs(X-OldX)/Step:0;PeakSpeed=FMath::Max(PeakSpeed,Speed);
    if(Elapsed>=Duration){Elapsed-=Duration;Phase=(Phase+1)%EWSkyrailPlan::PhaseCount(Line);if(Phase%2==0)++Arrivals;}
    const FVector Position=RenderOffset+EWSkyrailPlan::TrackPose(X,Line).GetLocation();
    Train->SetWorldLocation(Position);
    for(int I=0;I<2;++I){auto Pose=EWSkyrailPlan::TrackPose(X+(I?560:-560),Line);Pose.AddToTranslation(RenderOffset);CarFrames[I]->SetWorldTransform(Pose);}
    Distance=FMath::Abs(X-EWSkyrailPlan::StopX(0,Line));
    const double Want=AtStation()!=INDEX_NONE && M->ReadyAt(EWSkyrailPlan::StopChunk(AtStation(),Line)) && Elapsed<EWSkyrailPlan::Dwell-2?1.:0.;
    DoorOpen=FMath::FInterpConstantTo(DoorOpen,Want,Delta,1.5);
    for(int32 I=0;I<Doors.Num();++I)Doors[I]->SetRelativeLocation(FVector((I%2?1:-1)*(32+DoorOpen*66),0,0));
    if(auto* P=Rider.Get())
    {
        if(!P->IsSeated()){Rider.Reset();++Alightings;}
        else
        {
            const bool Locked=AtStation()==INDEX_NONE || DoorOpen<.95;
            P->UpdateTransitSeat(this,CarFrames[RiderCar]->GetComponentTransform().TransformPosition(FVector(-260,82,103)),StandAt(AtStation()==INDEX_NONE?BoardedAt:AtStation()),Locked);
        }
    }
    const float Scale=G->DayCycle?G->DayCycle->StreetLightScale():0;
    const float InsideScale=G->DayCycle?G->DayCycle->InteriorLightScale():0;
    const float SignPower=6000.f*InsideScale;
    for(auto Sign:Signs)Sign->SetTintColorAndOpacity(FLinearColor(SignPower,SignPower,SignPower,1));
    for(auto Guide:Guides)Guide->SetTintColorAndOpacity(FLinearColor(SignPower,SignPower,SignPower,1));
    if(Glow)Glow->SetScalarParameterValue(TEXT("Power"),2600*InsideScale);
    for(int32 I=0;I<Lights.Num();++I)
    {
        if(I>=2+EWSkyrailPlan::Count(Line)){Lights[I]->SetVisibility(false);Lights[I]->SetIntensity(0);continue;}
        const FVector Source=I<2?CarFrames[I]->GetComponentTransform().TransformPosition(FVector(0,0,275)):RenderOffset+EWSkyrailPlan::Stop(I-2,Line)+FVector(-450,420,282);
        if(I>=2)Lights[I]->SetWorldLocation(Source);
        const auto* P=UGameplayStatics::GetPlayerPawn(this,0);
        const bool Near=P && FVector::DistSquared(Source,P->GetActorLocation())<FMath::Square(6500.);
        const float LightScale=I<2?InsideScale:Scale;
        Lights[I]->SetIntensity(Near?LightScale*(I<2?160000:330000):0);Lights[I]->SetVisibility(Near && LightScale>0);
    }
    for(int32 I=0;I<Signs.Num();++I)
    {if(I>=EWSkyrailPlan::Count(Line)){Signs[I]->SetVisibility(false);continue;}Signs[I]->SetWorldLocation(RenderOffset+EWSkyrailPlan::Stop(I,Line)+FVector(1000,625,245));Signs[I]->SetVisibility(M->ReadyAt(EWSkyrailPlan::StopChunk(I,Line)));}
}
void AEWSkyrail::EndPlay(const EEndPlayReason::Type Reason){ReleaseRider();Super::EndPlay(Reason);}
bool AEWSkyrail::SetCinematicClock(double Seconds)
{
    const auto* G=GetGameInstance<UEWGameInstance>();
    FString Data,User;
    if(!G || !G->bScriptedWorldAudit || !FParse::Param(FCommandLine::Get(),TEXT("EWCinematic95")) ||
       !FParse::Value(FCommandLine::Get(),TEXT("EWDataDir="),Data) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),User) ||
       FPaths::IsRelative(Data) || FPaths::IsRelative(User) ||
       !FMath::IsFinite(Seconds) || Seconds<0 || Seconds>632 || Line<0 || Line>1 || Rider.IsValid())return false;
    double Cycle=0;for(int32 I=0;I<EWSkyrailPlan::PhaseCount(Line);++I)Cycle+=EWSkyrailPlan::Duration(I,Line);
    Elapsed=FMath::Fmod(Seconds,Cycle);Phase=0;
    while(Elapsed>=EWSkyrailPlan::Duration(Phase,Line))
    {Elapsed-=EWSkyrailPlan::Duration(Phase,Line);Phase=(Phase+1)%EWSkyrailPlan::PhaseCount(Line);}
    DoorOpen=Phase%2==0?FMath::Clamp(FMath::Min(Elapsed,EWSkyrailPlan::Dwell-2-Elapsed)*1.5,0.,1.):0.;
    bCinematicClock=true;bApplyingCinematicClock=true;
    // Reuse the real track, car, door, light and world-origin update once. The
    // actor's normal PrePhysics tick is held, so it cannot advance twice.
    Tick(0);
    bApplyingCinematicClock=false;
    return bReady;
}
bool AEWSkyrail::CinematicCarFrame(int32 Car,FTransform& Out) const
{
    if(!bCinematicClock || !bReady || !CarFrames.IsValidIndex(Car) || !CarFrames[Car])return false;
    Out=CarFrames[Car]->GetComponentTransform();return true;
}
void AEWSkyrail::ReleaseCinematicClock(){bCinematicClock=false;bApplyingCinematicClock=false;}
void AEWSkyrail::BuildGuides()
{
    auto* G=GetGameInstance<UEWGameInstance>();if(!G || !G->Manager)return;
    const auto R=G->Manager->RecipeAt({0,0});if(!R)return;
    auto Add=[&](FVector P,double Yaw,const FString& Text)
    {
        auto* W=NewObject<UWidgetComponent>(this);W->SetupAttachment(RootComponent);W->SetWidgetSpace(EWidgetSpace::World);
        W->SetDrawSize(FVector2D(900,240));W->SetPivot(FVector2D(.5,.5));W->SetTwoSided(true);W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        W->SetRelativeRotation(FRotator(0,Yaw,0));W->SetRelativeScale3D(FVector(.3));W->SetTickWhenOffscreen(false);W->RegisterComponent();
        W->SetSlateWidget(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.008,.03,.032,1)).Padding(18)
            [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",32)).AutoWrapText(true).ColorAndOpacity(FLinearColor(.85,.93,.86)).Text_Lambda([Text]{return FText::FromString(EWL::Translate(Text));})]);
        Guides.Add(W);GuidePositions.Add(P);
    };
    if(Line==0)
    {
        Add(FVector(11250,7030,3400),-90,TEXT("↑ ホテル・天空シアター・空港\n右の昇降機で『上層線』へ"));
        Add(FVector(11700,7390,R->Hub.Z+255),90,TEXT("時計広場駅\n水都線・上層線　この昇降機から"));
    }
    else
    {
        Add(FVector(11250,7030,30260),-90,TEXT("↑ 天空シアター　↓ 時計広場・水都線\n右の昇降機で行き先を選ぶ"));
        for(int Level=0;Level<2;++Level)
        {
            const auto A=EWSkyrailPlan::HotelPath(*R,Level*2),B=EWSkyrailPlan::HotelPath(*R,4+Level*2);
            if(A.IsEmpty() || B.IsEmpty())continue;
            Add(FVector(5940,6820,A[0].Z+210),0,Level?TEXT("雲上ホテル\n← 107・108　　　103・104 →"):TEXT("雲上ホテル\n← 105・106　　　101・102 →"));
            for(int Wing=0;Wing<2;++Wing)
            {
                const int I=Wing*4+Level*2;const auto& Path=Wing?B:A;
                Add(Path[3]+FVector(5,0,210),0,FString::Printf(TEXT("%d %s\n%d %s"),101+I,*EWHotelPlan::Name(I),102+I,*EWHotelPlan::Name(I+1)));
            }
        }
        Add(FVector(24325,5790,30260),90,TEXT("アウレリア空中港\nこの昇降機で『搭乗口』へ"));
    }
    bGuidesBuilt=true;
}
TSharedRef<FJsonObject> AEWSkyrail::Evidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("line"),Line);O->SetBoolField(TEXT("ready"),bReady);O->SetNumberField(TEXT("phase"),Phase);
    O->SetNumberField(TEXT("station"),AtStation());O->SetNumberField(TEXT("elapsed"),Elapsed);O->SetNumberField(TEXT("door_open"),DoorOpen);
    O->SetNumberField(TEXT("distance_cm"),Distance);O->SetNumberField(TEXT("route_cm"),EWSkyrailPlan::StopX(EWSkyrailPlan::Count(Line)-1,Line)-EWSkyrailPlan::StopX(0,Line));
    O->SetNumberField(TEXT("stations"),EWSkyrailPlan::Count(Line));O->SetNumberField(TEXT("speed_kmh"),Speed*.036);O->SetNumberField(TEXT("peak_speed_kmh"),PeakSpeed*.036);
    O->SetNumberField(TEXT("first_leg_seconds"),EWSkyrailPlan::Duration(1,Line));O->SetNumberField(TEXT("water_leg_seconds"),EWSkyrailPlan::Duration(3,Line));
    O->SetNumberField(TEXT("boardings"),Boardings);O->SetNumberField(TEXT("arrivals"),Arrivals);O->SetNumberField(TEXT("alightings"),Alightings);
    O->SetNumberField(TEXT("rider_car"),RiderCar);
    O->SetNumberField(TEXT("collision_bodies"),Collision.Num());O->SetNumberField(TEXT("streamline_meshes"),BodyDetails.Num());
    O->SetBoolField(TEXT("riding"),IsRider(Rider.Get()));O->SetBoolField(TEXT("car_mesh"),Cars.Num()==4 && Cars[0]->GetStaticMesh()!=nullptr);
    O->SetStringField(TEXT("destination"),EWSkyrailPlan::Name(Destination(),Line));return O;
}
FString AEWSkyrail::RailEvidence() const {FString S;FJsonSerializer::Serialize(Evidence(),TJsonWriterFactory<>::Create(&S));return S;}
bool AEWSkyrail::PreviewStation(int32 I,bool Exterior)
{
#if WITH_EDITOR
    if(!GIsEditor || !bReady || I<0 || I>=EWSkyrailPlan::Count(Line))return false;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!P || !PC)return false;ReleaseRider();P->LeaveSeat();P->ClearHeldInput();Phase=I*2;Elapsed=2;Tick(0);
    P->SetActorLocation(BoardingPosition(I),false,nullptr,ETeleportType::TeleportPhysics);P->ResetSafeLocation();PC->SetControlRotation(FRotator(0,270,0));
    if(Exterior)
    {
        if(!PreviewCamera)PreviewCamera=GetWorld()->SpawnActor<ACameraActor>();
        const FVector Target=RenderOffset+EWSkyrailPlan::Stop(I,Line)+FVector(-200,0,130);
        const FVector Eye=Target+FVector(-1800,1300,500);
        PreviewCamera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation());PreviewCamera->GetCameraComponent()->SetFieldOfView(80);PC->SetViewTarget(PreviewCamera);
    }
    else PC->SetViewTarget(P);
    return true;
#else
    return false;
#endif
}
