#include "EWNightLighting.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWDayCycle.h"
#include "EWOuterWater.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"

namespace { constexpr int32 LanternBudget=12,ArchBudget=6; }
AEWNightLighting::AEWNightLighting()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("NightLightingRoot")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Lamp(TEXT("/Game/EndlessWorld/Kit/SM_Lamp.SM_Lamp"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Bronze(TEXT("/Game/EndlessWorld/Materials/M_Bronze.M_Bronze"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glass(TEXT("/Game/EndlessWorld/Materials/M_NightFixtureGlow.M_NightFixtureGlow"));
    Lanterns=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GalleryLanterns"));
    Housings=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ArchProjectorHousings"));
    Lenses=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ArchProjectorLenses"));
    for(auto* Group:{Lanterns.Get(),Housings.Get(),Lenses.Get()})
    {
        Group->SetupAttachment(RootComponent);Group->SetMobility(EComponentMobility::Movable);
        Group->SetCollisionEnabled(ECollisionEnabled::NoCollision);Group->SetCanEverAffectNavigation(false);
        Group->SetCullDistances(18000,24000);Group->SetCastShadow(Group!=Lenses.Get());
    }
    Lanterns->SetStaticMesh(Lamp.Object);Housings->SetStaticMesh(Cube.Object);Lenses->SetStaticMesh(Cube.Object);
    Housings->SetMaterial(0,Bronze.Object);Lenses->SetMaterial(0,Glass.Object);
    for(int32 I=0;I<LanternBudget+ArchBudget;++I)
    {
        UPointLightComponent* Light=nullptr;
        if(I<LanternBudget)Light=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("GalleryPool_%d"),I));
        else
        {
            auto* Spot=CreateDefaultSubobject<USpotLightComponent>(*FString::Printf(TEXT("ArchPool_%d"),I-LanternBudget));
            Spot->SetInnerConeAngle(22);Spot->SetOuterConeAngle(46);Light=Spot;
        }
        Light->SetupAttachment(RootComponent);Light->SetMobility(EComponentMobility::Movable);
        Light->SetIntensityUnits(ELightUnits::Lumens);Light->SetLightColor(FLinearColor(1.f,.72f,.43f));
        Light->SetSourceRadius(I<LanternBudget?12.f:8.f);Light->SetSoftSourceRadius(20);
        Light->SetCastShadows(true);Light->SetVolumetricScatteringIntensity(0);Light->SetIndirectLightingIntensity(.65f);
        Light->SetIntensity(0);Light->SetVisibility(false);Lights.Add(Light);
    }
    Slots.SetNum(Lights.Num());
}
void AEWNightLighting::BeginPlay()
{
    Super::BeginPlay();Glow=Lenses->CreateDynamicMaterialInstance(0);
    if(Glow && Lanterns->GetStaticMesh())
    {
        const auto& Materials=Lanterns->GetStaticMesh()->GetStaticMaterials();
        for(int32 I=0;I<Materials.Num();++I)
            if(Materials[I].MaterialInterface && Materials[I].MaterialInterface->GetName().Contains(TEXT("Glow")))Lanterns->SetMaterial(I,Glow);
    }
}
bool AEWNightLighting::PreviewNightLights(float Gain)
{
#if WITH_EDITOR
    if(GIsEditor && FMath::IsFinite(Gain) && Gain>=0 && Gain<=3){PreviewGain=Gain;return true;}
#endif
    return false;
}
void AEWNightLighting::Tick(float Delta)
{
    Super::Tick(Delta);
    const auto* G=GetGameInstance<UEWGameInstance>();const auto* M=G?G->Manager.Get():nullptr;
    const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
    if(!M || !Camera)return;
    const FVector Eye=Camera->GetCameraLocation();const auto Centre=M->PlayerCoord();
    FString Key=M->Descriptor().Code()+TEXT("/")+Centre.Text()+TEXT("/")+M->OriginCoord().Text()+FString::Printf(TEXT("/%d"),FMath::FloorToInt(Eye.Z/1800.));
    for(int32 Y=-1;Y<=1;++Y)for(int32 X=-1;X<=1;++X)
    {const auto R=M->RecipeAt(Centre+EW::ChunkCoord{X,Y});Key+=FString::Printf(TEXT("/%p"),R.Get());}
    if(Key!=CacheKey)
    {
        CacheKey=Key;++Rebuilds;Fixtures.Reset();Lanterns->ClearInstances();Housings->ClearInstances();Lenses->ClearInstances();
        for(int32 Y=-1;Y<=1;++Y)for(int32 X=-1;X<=1;++X)
        {
            const auto Coord=Centre+EW::ChunkCoord{X,Y};const auto R=M->RecipeAt(Coord);if(!R)continue;
            if(R->bCascadeCity)
            {
                int32 I=0;for(const auto Local:EWOuterWater::Lamps(*R))
                {
                    const auto Base=M->ToRender(Coord,Local);const FTransform Body(FRotator::ZeroRotator,Base,FVector(.62));
                    Lanterns->AddInstance(Body,true);Fixtures.Add({Coord.Text()+FString::Printf(TEXT("/cascade86/%d"),I++),Body.TransformPosition(FVector(0,-48,418)),FRotator::ZeroRotator,4000000,2000,false});
                }
                // Graze the palace ribs from small cornice-mounted projectors.
                // Reuse the existing six shadow slots; distant towers add no lights.
                int32 Tower=0;for(const auto& Part:R->Parts)if(Part.Mesh.ToString().StartsWith(TEXT("Cascade88Palace")))
                {
                    for(int32 Side=0;Side<4;++Side)
                    {
                        const FRotator Face(0,Side*90.,0);
                        const FVector Mount=Part.Transform.TransformPosition(Face.RotateVector(FVector(0,-1320,1800)));
                        const FVector Target=Part.Transform.TransformPosition(Face.RotateVector(FVector(0,-1180,4600)));
                        const FVector Position=M->ToRender(Coord,Mount);const FRotator Aim=(Target-Mount).Rotation();
                        Housings->AddInstance(FTransform(Aim,Position,FVector(.30,.26,.22)),true);
                        Lenses->AddInstance(FTransform(Aim,Position+Aim.Vector()*19,FVector(.025,.22,.18)),true);
                        Fixtures.Add({Coord.Text()+FString::Printf(TEXT("/palace88/%d/%d"),Tower,Side),Position+Aim.Vector()*27,Aim,2800000,4400,true});
                    }
                    ++Tower;
                }
                continue;
            }
            if(R->Region!=EW::RegionKind::City)continue;
            int32 FloorIndex=0;
            for(const auto& Floor:R->CityFloors)
            {
                const int32 Index=FloorIndex++;
                if(FMath::Abs(Floor.Transform.GetLocation().Z-Eye.Z)>9000)continue;
                // One slender lantern on each face. Alternate its side by
                // public floor so distant galleries do not form a light grid.
                for(int32 Side=0;Side<4;++Side)
                {
                    const auto& T=Floor.Transform;const FVector Scale=T.GetScale3D();
                    const FTransform Face(T.GetRotation()*FRotator(0,90.*Side,0).Quaternion(),T.GetLocation(),Side%2?FVector(Scale.Y,Scale.X,Scale.Z):Scale);
                    const double Along=((Index+Side+Floor.Column)%2?1.:-1.)*1320.;
                    const FVector Base=M->ToRender(Coord,Face.TransformPosition(FVector(Along,-2180,0)));
                    const FTransform Body(Face.GetRotation(),Base,FVector(.62));Lanterns->AddInstance(Body,true);
                    Fixtures.Add({Coord.Text()+FString::Printf(TEXT("/gallery/%d/%d"),Index,Side),
                        Body.TransformPosition(FVector(0,-48,418)),FRotator::ZeroRotator,1250000,1450,false});
                }
            }
            int32 PartIndex=0;
            for(const auto& Part:R->Parts)
            {
                const int32 Index=PartIndex++;const FString Name=Part.Mesh.ToString();
                if(!Name.StartsWith(TEXT("UrbanBridge_")) && !Name.StartsWith(TEXT("UrbanPromenade_")))continue;
                const auto& T=Part.Transform;if(FMath::Abs(T.GetLocation().Z-Eye.Z)>11000)continue;
                const bool Promenade=Name.StartsWith(TEXT("UrbanPromenade_"));
                const double Width=Promenade?350.:266.4;
                const double Drop=Promenade?600.:1250.;
                // Bracket-mounted projectors graze the outer surface of each
                // load-bearing rib. Both the bracket and source share its pose.
                for(double Sign:{-1.,1.})
                {
                    const double LX=Sign*2750.,LY=Sign*Width;
                    const double LZ=-135.-Drop*FMath::Square(LX/3200.);
                    const FVector Mount=T.TransformPosition(FVector(LX,LY+Sign*68,LZ));
                    const FVector Target=T.TransformPosition(FVector(Sign*750,LY,-135.-Drop*FMath::Square(750./3200.)));
                    const FRotator Aim=(Target-Mount).Rotation();
                    const FVector Position=M->ToRender(Coord,Mount);
                    Housings->AddInstance(FTransform(Aim,Position,FVector(.32,.27,.24)),true);
                    const FVector Face=Position+Aim.Vector()*19;
                    Lenses->AddInstance(FTransform(Aim,Face,FVector(.025,.22,.18)),true);
                    Fixtures.Add({Coord.Text()+FString::Printf(TEXT("/arch/%d/%d"),Index,Sign>0?1:0),
                        Position+Aim.Vector()*27,Aim,3500000,4200,true});
                }
            }
        }
    }
    LastNightScale=G->DayCycle?G->DayCycle->StreetLightScale()*PreviewGain:0;
    if(Glow)Glow->SetScalarParameterValue(TEXT("Power"),3000.f*LastNightScale);
    // Keep the same source in each shadow slot until it has faded completely
    // out. Rank hysteresis prevents swapping when two lanterns are equidistant.
    for(int32 Type=0;Type<2;++Type)
    {
        const int32 Start=Type?LanternBudget:0,End=Type?Lights.Num():LanternBudget;
        TArray<int32> Order;
        for(int32 I=0;I<Fixtures.Num();++I)
            if(Fixtures[I].Spot==bool(Type) && FVector::DistSquared(Fixtures[I].Position,Eye)<FMath::Square(Type?12500.:6500.))Order.Add(I);
        auto Score=[&](int32 Index)
        {
            double D=FVector::DistSquared(Fixtures[Index].Position,Eye);
            if(Type)
            {
                // Spend the six grazing shadows on ribs in the visible street
                // canyon, rather than on equally near arches below the floor.
                const FVector View=Camera->GetCameraRotation().Vector();
                const FVector Offset=Fixtures[Index].Position-Eye;
                const float Facing=FVector::DotProduct(View,Offset.GetSafeNormal());
                D*=FMath::Lerp(4.f,.65f,FMath::SmoothStep(-.45f,.55f,Facing));
                if(View.Z>-.2f && Offset.Z<-650)D*=4.;
            }
            for(int32 I=Start;I<End;++I)if(Slots[I].Id==Fixtures[Index].Id){D*=.78;break;}
            return D;
        };
        Order.Sort([&](int32 A,int32 B){return Score(A)<Score(B);});
        if(Order.Num()>End-Start)Order.SetNum(End-Start);
        for(int32 I=Start;I<End;++I)
        {
            auto& Slot=Slots[I];auto* Light=Lights[I].Get();
            const bool Wanted=Order.ContainsByPredicate([&](int32 J){return Fixtures[J].Id==Slot.Id;});
            Slot.Fade=FMath::FInterpConstantTo(Slot.Fade,Wanted?1.f:0.f,Delta,2.f);
            if(!Wanted && Slot.Fade<=0)
            {
                Slot.Id.Reset();
                for(int32 J:Order)
                {
                    bool Used=false;for(int32 K=Start;K<End;++K)if(Slots[K].Id==Fixtures[J].Id){Used=true;break;}
                    if(!Used){Slot.Id=Fixtures[J].Id;break;}
                }
            }
            const auto* F=Fixtures.FindByPredicate([&](const FEWNightFixture& V){return V.Id==Slot.Id;});
            if(!F || LastNightScale<=0){Light->SetVisibility(false);Light->SetIntensity(0);continue;}
            const float Distance=FVector::Distance(F->Position,Eye);
            const float DistanceFade=1.f-FMath::SmoothStep(Type?9000.f:4500.f,Type?12500.f:6500.f,Distance);
            Light->SetWorldLocationAndRotation(F->Position,F->Rotation);Light->SetAttenuationRadius(F->Radius);
            Light->SetIntensity(F->Lumens*LastNightScale*Slot.Fade*DistanceFade);Light->SetVisibility(Light->Intensity>.01f);
        }
    }
}
TSharedRef<FJsonObject> AEWNightLighting::Evidence() const
{
    auto O=MakeShared<FJsonObject>();int32 Active=0,Spots=0;bool Shadows=true;double Lumens=0;
    for(const auto& Ptr:Lights)if(const auto* L=Ptr.Get();L && L->IsVisible())
    {++Active;Spots+=L->IsA<USpotLightComponent>();Shadows&=L->CastShadows;Lumens+=L->Intensity;}
    O->SetNumberField(TEXT("fixture_count"),Fixtures.Num());O->SetNumberField(TEXT("active_lights"),Active);
    O->SetNumberField(TEXT("active_arch_spots"),Spots);O->SetNumberField(TEXT("shadow_light_budget"),LanternBudget+ArchBudget);
    O->SetBoolField(TEXT("all_cast_shadows"),Shadows);O->SetNumberField(TEXT("lumens_sum"),Lumens);
    O->SetNumberField(TEXT("night_scale"),LastNightScale);O->SetNumberField(TEXT("preview_gain"),PreviewGain);
    O->SetBoolField(TEXT("night_glow_material"),Glow!=nullptr);O->SetNumberField(TEXT("fixture_rebuilds"),Rebuilds);
    for(const TCHAR* Name:{TEXT("r.RHICmdWidth"),TEXT("r.RHICmd.ParallelTranslate.MaxCommandsPerTranslate"),TEXT("r.RHICmd.ParallelTranslate.Enable"),TEXT("r.RDG.ParallelExecute")})
        if(const auto* C=IConsoleManager::Get().FindConsoleVariable(Name))O->SetNumberField(Name,C->GetInt());
    return O;
}
