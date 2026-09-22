#include "EWAirship.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWDayCycle.h"
#include "EWAeroYachtPlan.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Dom/JsonObject.h"

AEWAirship::AEWAirship()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AirshipRoot")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Hull(TEXT("/Game/EndlessWorld/Kit/SM_Airship82Hull.SM_Airship82Hull"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Glass(TEXT("/Game/EndlessWorld/Kit/SM_Airship82Glass.SM_Airship82Glass"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Prop(TEXT("/Game/EndlessWorld/Kit/SM_Airship82Prop.SM_Airship82Prop"));
    for(int I=0;I<2;++I)
    {
        auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("AirshipMesh%d"),I));
        C->SetupAttachment(RootComponent);C->SetStaticMesh(I?Glass.Object:Hull.Object);C->SetMobility(EComponentMobility::Movable);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(I==0);Meshes.Add(C);
    }
    for(int I=0;I<4;++I)
    {
        auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Propeller%d"),I));
        C->SetupAttachment(RootComponent);C->SetStaticMesh(Prop.Object);C->SetRelativeLocation(FVector(I<2?-1105:795,I%2?550:-550,-230));
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);Props.Add(C);
    }
}
void AEWAirship::BeginPlay()
{
    Super::BeginPlay();Started=FPlatformTime::Seconds();SetActorHiddenInGame(true);
    if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_NightFixtureGlow.M_NightFixtureGlow")))
    {
        Glow=UMaterialInstanceDynamic::Create(Material,this);
        for(const auto& C:Meshes)for(int I=0;I<C->GetNumMaterials();++I)if(C->GetMaterial(I) && C->GetMaterial(I)->GetName().Contains(TEXT("Glow")))C->SetMaterial(I,Glow);
    }
}
void AEWAirship::Tick(float Delta)
{
    Super::Tick(Delta);const auto* G=GetGameInstance<UEWGameInstance>();const auto* M=G?G->Manager.Get():nullptr;
    Ready=M && M->bHasWorld && M->Descriptor().Seed==EW::WorldDescriptor::ReferenceWorld().Seed;
    SetActorHiddenInGame(!Ready);if(!Ready)return;
    if(CruiseZ==0)
    {
        // Find a clear band above the actual roofs crossed by the complete hull.
        // The orbit stays over these two streamed city blocks, between taller distant towers.
        double Top=0;
        for(int I=0;I<2;++I)if(const auto R=M->RecipeAt({I,0}))for(const auto& C:R->Colliders)
        {
            const FBox Bounds=FBox(-C.Extent,C.Extent).TransformBy(C.Transform);
            const FBox Lane(FVector(2200-I*EW::ChunkSize,2100,-100000),FVector(23400-I*EW::ChunkSize,10700,100000));
            if(Bounds.Intersect(Lane))Top=FMath::Max(Top,Bounds.Max.Z);
        }
        CruiseZ=FMath::Max(Top+1250,EWAeroYachtPlan::Design().Ceiling+8000+FleetIndex*1800);
    }
    const double Age=FPlatformTime::Seconds()-Started,A=Age*2*PI/220.+FleetIndex*2*PI/3.;
    const FVector Local(12800+7800*FMath::Cos(A),6400+2700*FMath::Sin(A),CruiseZ+45*FMath::Sin(A*2));
    const FVector Direction(-7800*FMath::Sin(A),2700*FMath::Cos(A),0);
    SetActorLocationAndRotation(M->ToRender({0,0},Local),Direction.Rotation()+FRotator(.6*FMath::Sin(A),0,1.2*FMath::Cos(A)));
    for(int I=0;I<Props.Num();++I)Props[I]->SetRelativeRotation(FRotator(0,0,FMath::Fmod(Age*(I%2?330:-330),360.)));
    if(Glow)Glow->SetScalarParameterValue(TEXT("Power"),1800*(G->DayCycle?G->DayCycle->InteriorLightScale():1));
}
TSharedRef<FJsonObject> AEWAirship::Evidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("ready"),Ready);O->SetNumberField(TEXT("cruise_z"),CruiseZ);
    O->SetNumberField(TEXT("fleet_index"),FleetIndex);O->SetNumberField(TEXT("period_seconds"),220);O->SetStringField(TEXT("position"),GetActorLocation().ToString());
    O->SetBoolField(TEXT("hull_loaded"),Meshes.Num()==2 && Meshes[0]->GetStaticMesh());return O;
}
