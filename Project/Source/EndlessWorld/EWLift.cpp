#include "EWLift.h"
#include "EWCharacter.h"
#include "EWGameInstance.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

AEWLift::AEWLift()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;
    Root=CreateDefaultSubobject<USceneComponent>(TEXT("LiftRoot"));SetRootComponent(Root);
    Cabin=CreateDefaultSubobject<USceneComponent>(TEXT("MovingCabin"));Cabin->SetupAttachment(Root);
}
void AEWLift::Initialize(const EW::LiftSpec& InSpec,double PlayerHeight)
{
    Spec=InSpec;if(Spec.Stops.IsEmpty())return;
    CurrentStop=0;
    for(int32 I=1;I<Spec.Stops.Num();++I)
        if(FMath::Abs(Spec.Stops[I].Height-PlayerHeight)<FMath::Abs(Spec.Stops[CurrentStop].Height-PlayerHeight))CurrentStop=I;
    Cabin->SetRelativeLocation(FVector(Spec.Cabin.X,Spec.Cabin.Y,Spec.Stops[CurrentStop].Height));
    Cabin->SetRelativeRotation(Spec.Stops[CurrentStop].BoardingRotation);
    auto Box=[&](USceneComponent* Parent,FVector P,FVector Extent,bool Walk=false)
    {
        auto* B=NewObject<UBoxComponent>(this);B->SetupAttachment(Parent);B->SetMobility(EComponentMobility::Movable);
        B->SetRelativeLocation(P);B->SetBoxExtent(Extent);B->SetCollisionObjectType(ECC_WorldDynamic);
        B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);B->SetCollisionResponseToAllChannels(ECR_Block);
        B->SetGenerateOverlapEvents(false);B->SetCanEverAffectNavigation(false);
        B->ComponentTags.Add(Walk?TEXT("EWFloor"):TEXT("EWObstacle"));B->RegisterComponent();return B;
    };
    Floor=Box(Cabin,FVector(0,0,-18),FVector(200,200,18),true);
    Box(Cabin,FVector(0,-190,62),FVector(190,12,62));
    for(double X:{-190.,190.})Box(Cabin,FVector(X,0,62),FVector(12,190,62));
    Box(Cabin,FVector(0,0,320),FVector(215,215,12.5));
    CarGate=Box(Cabin,FVector(0,190,62),FVector(190,12,62));
    auto* M=NewObject<UStaticMeshComponent>(this);M->SetupAttachment(Cabin);M->SetMobility(EComponentMobility::Movable);
    // FBX import reflects the kit's Y axis: the cooked cabin opens at -Y.
    // Collision and every landing use +Y. Rotate only its visual mesh so a
    // passenger sees the same open doorway that CharacterMovement traverses.
    M->SetRelativeRotation(FRotator(0,180,0));
    M->SetStaticMesh(Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Game/EndlessWorld/Kit/SM_UrbanLiftCabin.SM_UrbanLiftCabin")).ResolveObject()));
    M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->RegisterComponent();
    auto* Rail=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndlessWorld/Kit/SM_Wall.SM_Wall"));
    GateMeshes=NewObject<UInstancedStaticMeshComponent>(this);GateMeshes->SetupAttachment(Root);
    GateMeshes->SetMobility(EComponentMobility::Movable);GateMeshes->SetStaticMesh(Rail);
    GateMeshes->SetCollisionEnabled(ECollisionEnabled::NoCollision);GateMeshes->RegisterComponent();
    for(const auto& S:Spec.Stops)
    {
        const FVector P=S.Threshold;
        auto* B=Box(Root,P+FVector(0,0,62),FVector(145,12,62));B->SetRelativeRotation(S.BoardingRotation);Doors.Add(B);
        AddGate(Doors.Num()-1,145);
    }
    // Last instance is the car's boarding guard; its transform follows the car.
    AddGate(Doors.Num(),190);UpdateDoors();
}
void AEWLift::AddGate(int32 Stop,double HalfWidth)
{
    // A dedicated frame, not a scaled balustrade with a large centre post.
    auto Add=[&](FVector P,FVector Size)
    {GatePieces.Add({GateMeshes->AddInstance(FTransform::Identity),Stop,P,Size});};
    for(double H:{18.,116.})Add(FVector(0,0,H),FVector(HalfWidth*2,12,14));
    const int32 Bars=FMath::CeilToInt(HalfWidth*2/42.);
    for(int32 I=0;I<=Bars;++I)Add(FVector(FMath::Lerp(-HalfWidth,HalfWidth,double(I)/Bars),0,67),FVector(8,10,90));
}
void AEWLift::DrawGates(bool CarOnly)
{
    for(const auto& P:GatePieces)
    {
        const bool Car=P.Stop==Doors.Num();
        if(CarOnly && !Car)continue;
        const bool Closed=IsMoving() || (!Car && P.Stop!=CurrentStop);
        const FQuat Q=Car?Cabin->GetRelativeRotation().Quaternion():Spec.Stops[P.Stop].BoardingRotation.Quaternion();
        const FVector Origin=Car?Cabin->GetRelativeLocation()+Q.RotateVector(FVector(0,190,0)):Spec.Stops[P.Stop].Threshold;
        GateMeshes->UpdateInstanceTransform(P.Instance,FTransform(Q,Origin+Q.RotateVector(P.Centre),Closed?P.Size/100.:FVector(.0001)),false,false,true);
    }
    GateMeshes->MarkRenderStateDirty();
}
void AEWLift::UpdateDoors()
{
    for(int32 I=0;I<Doors.Num();++I)
    {
        const bool Closed=IsMoving() || I!=CurrentStop;
        Doors[I]->SetCollisionEnabled(Closed?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    }
    CarGate->SetCollisionEnabled(IsMoving()?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    DrawGates();
}
FVector AEWLift::CarPosition() const{return Cabin->GetComponentLocation();}
FVector AEWLift::LandingPosition(int32 Stop) const
{return Spec.Stops.IsValidIndex(Stop)?GetActorTransform().TransformPosition(Spec.Stops[Stop].Landing):GetActorLocation();}
bool AEWLift::Nearby(const AEWCharacter* P,int32& Stop,bool& OnCar) const
{
    Stop=INDEX_NONE;OnCar=false;if(!P)return false;
    const FVector Pos=P->GetActorLocation();
    const FVector C=Cabin->GetComponentTransform().InverseTransformPosition(Pos);
    const double Feet=C.Z-P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    OnCar=FMath::Abs(C.X)<145 && FMath::Abs(C.Y)<145 && FMath::Abs(Feet)<25;
    if(OnCar){Stop=CurrentStop;return true;}
    for(int32 I=0;I<Spec.Stops.Num();++I)
    {
        const FVector L=LandingPosition(I);
        if(FVector::Dist2D(L,Pos)<360 && FMath::Abs(Pos.Z-88-L.Z)<140){Stop=I;return true;}
    }
    return false;
}
bool AEWLift::Call(int32 Stop)
{
    if(IsMoving() || !Spec.Stops.IsValidIndex(Stop))return false;
    if(Stop==CurrentStop)return true;
    StartHeight=Cabin->GetRelativeLocation().Z;TargetHeight=Spec.Stops[Stop].Height;Elapsed=0;
    StartRotation=Cabin->GetRelativeRotation().Quaternion();TargetRotation=Spec.Stops[Stop].BoardingRotation.Quaternion();
    // Smooth acceleration/deceleration; longest calls are still bounded by the tower height.
    Duration=FMath::Max(2.,FMath::Abs(TargetHeight-StartHeight)/1800.+1.5);
    TargetStop=Stop;UpdateDoors();return true;
}
bool AEWLift::Ride(AEWCharacter* P,int32 Stop)
{
    int32 Here;bool OnCar;
    if(!Nearby(P,Here,OnCar) || !OnCar || IsMoving() || Stop==CurrentStop || !Spec.Stops.IsValidIndex(Stop))return false;
    P->ClearHeldInput();P->SetLiftRiding(true);P->SetBase(static_cast<UPrimitiveComponent*>(nullptr));
    P->AttachToComponent(Cabin,FAttachmentTransformRules::KeepWorldTransform);Rider=P;
    if(!Call(Stop)){P->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);P->SetLiftRiding(false);Rider.Reset();return false;}
    return true;
}
FString AEWLift::Hint(const AEWCharacter* P) const
{
    int32 Stop;bool OnCar;if(!Nearby(P,Stop,OnCar))return {};
    if(IsMoving())return TEXT("展望昇降機　移動中…");
    if(OnCar)return TEXT("展望昇降機　行き先の階を選ぶ");
    if(Spec.Id.StartsWith(TEXT("skyrail")))return Spec.Stops[Stop].Label+(CurrentStop==Stop?TEXT("　開いたかごへ進む"):TEXT("　この階へ呼ぶ"));
    return CurrentStop==Stop?TEXT("展望昇降機　かごの中央へ進む"):TEXT("展望昇降機　この階へ呼ぶ");
}
void AEWLift::Tick(float Delta)
{
    if(bCinematicTrip && !bApplyingCinematicTrip)return;
    Super::Tick(Delta);if(!IsMoving())return;
    Elapsed+=Delta;const double T=FMath::Clamp(Elapsed/Duration,0.,1.);
    const double Ease=T*T*(3-2*T);
    FVector P=Cabin->GetRelativeLocation();P.Z=FMath::Lerp(StartHeight,TargetHeight,Ease);Cabin->SetRelativeLocation(P);
    const FQuat Q=FQuat::Slerp(StartRotation,TargetRotation,Ease);Cabin->SetRelativeRotation(Q);
    DrawGates(true);
    if(T<1)return;
    CurrentStop=TargetStop;TargetStop=INDEX_NONE;UpdateDoors();
    if(bCinematicTrip)return;
    if(auto* Player=Rider.Get())
    {
        Player->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);Player->SetLiftRiding(false);
        Player->SetBase(Floor);Player->GetCharacterMovement()->bForceNextFloorCheck=true;Player->ResetSafeLocation();
        ++CompletedRides;Rider.Reset();
    }
    else ++CompletedCalls;
    if(auto* GI=GetGameInstance<UEWGameInstance>())GI->Notify(TEXT("展望昇降機：")+Spec.Stops[CurrentStop].Label);
}
bool AEWLift::SetCinematicTrip(int32 FromStop,int32 ToStop,double Seconds)
{
    const auto* G=GetGameInstance<UEWGameInstance>();FString Data,User;
    if(!G || !G->bScriptedWorldAudit || !FParse::Param(FCommandLine::Get(),TEXT("EWCinematic96")) ||
       !FParse::Param(FCommandLine::Get(),TEXT("EWCinematic95")) ||
       !FParse::Value(FCommandLine::Get(),TEXT("EWDataDir="),Data) || !FParse::Value(FCommandLine::Get(),TEXT("UserDir="),User) ||
       FPaths::IsRelative(Data) || FPaths::IsRelative(User) || Rider.IsValid() || !Cabin || !CarGate ||
       !Spec.Stops.IsValidIndex(FromStop) || !Spec.Stops.IsValidIndex(ToStop) || FromStop==ToStop ||
       !FMath::IsFinite(Seconds) || Seconds<0 || Seconds>632)return false;
    // Each timestamp uses the real Call/Tick trajectory. Hold the automatic
    // PrePhysics tick so warmup cannot consume any of the filmed journey.
    CurrentStop=FromStop;TargetStop=INDEX_NONE;
    Cabin->SetRelativeLocation(FVector(Spec.Cabin.X,Spec.Cabin.Y,Spec.Stops[FromStop].Height));
    Cabin->SetRelativeRotation(Spec.Stops[FromStop].BoardingRotation);
    if(!Call(ToStop))return false;
    Elapsed=Seconds;bCinematicTrip=true;bApplyingCinematicTrip=true;Tick(0);bApplyingCinematicTrip=false;
    return true;
}
bool AEWLift::CinematicCabinFrame(FTransform& Out) const
{if(!bCinematicTrip || !Cabin)return false;Out=Cabin->GetComponentTransform();return true;}
void AEWLift::ReleaseCinematicTrip(){bCinematicTrip=false;bApplyingCinematicTrip=false;}
void AEWLift::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* P=Rider.Get()){P->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);P->SetLiftRiding(false);P->ResetSafeLocation();}
    Rider.Reset();Super::EndPlay(Reason);
}
