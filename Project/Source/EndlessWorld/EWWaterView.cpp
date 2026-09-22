#include "EWWaterView.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWDayCycle.h"
#include "EWAeroYacht.h"
#include "EWWaterCity.h"
#include "EWOuterWater.h"
#include "Components/PostProcessComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

AEWWaterView::AEWWaterView()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WaterViewRoot")));
    Optical=CreateDefaultSubobject<UPostProcessComponent>(TEXT("UnderwaterOptics"));Optical->SetupAttachment(RootComponent);
    Optical->bUnbound=true;Optical->Priority=600;Optical->BlendWeight=0;
    for(int32 I=0;I<6;++I)
    {
        auto* L=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("PoolLamp%d"),I));
        L->SetupAttachment(RootComponent);L->SetMobility(EComponentMobility::Movable);L->SetIntensityUnits(ELightUnits::Lumens);
        L->SetLightColor(FLinearColor(.28,.68,.77));L->SetAttenuationRadius(650);L->SetSourceRadius(16);
        L->SetCastShadows(true);L->SetVolumetricScatteringIntensity(0);L->SetIntensity(0);L->SetVisibility(false);Lamps.Add(L);
    }
}
void AEWWaterView::BeginPlay()
{
    Super::BeginPlay();auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_Pool90Underwater.M_Pool90Underwater"));
    if(Parent){Material=UMaterialInstanceDynamic::Create(Parent,this);Optical->AddOrUpdateBlendable(Material,1);}
}
bool AEWWaterView::Sample(const FVector& Eye,FEWWaterSample& Out) const
{
    const auto* G=GetGameInstance<UEWGameInstance>();const auto* M=G?G->Manager.Get():nullptr;
    if(!M || M->IsTravelling())return false;
    auto InBox=[&](const FString& Id,FTransform Frame,FVector Extent,double Bottom)
    {
        const FVector P=Frame.InverseTransformPositionNoScale(Eye);
        if(FMath::Abs(P.X)>=Extent.X || FMath::Abs(P.Y)>=Extent.Y || P.Z>=Extent.Z || P.Z<=Bottom)return false;
        Out.Id=Id;Out.Frame=Frame;Out.Extent=Extent;Out.Bottom=Bottom;Out.Depth=Extent.Z-P.Z;return true;
    };
    if(M->Descriptor().Seed==EW::WorldDescriptor::ReferenceWorld().Seed)
    {
        for(const auto& P:Sites)
        {
            if(!M->ReadyAt(P.Coord))continue;
            const FTransform Frame(P.Frame.GetRotation(),M->ToRender(P.Coord,P.Frame.GetLocation()));
            const FVector Scale=P.Frame.GetScale3D();
            if(InBox(TEXT("roof/")+FString::FromInt(P.Index),Frame,FVector(EWPoolPlan::HalfX*Scale.X,EWPoolPlan::HalfY*Scale.Y,EWPoolPlan::Surface),EWPoolPlan::Bottom))return true;
        }
        for(const auto& Yacht:G->Yachts)if(Yacht && Yacht->WalkingBody() && Yacht->WalkingBody()->IsCollisionEnabled())
        {
            // A moving pool is sampled in its ship frame, including origin shifts.
            FTransform Frame=Yacht->GetActorTransform();Frame.SetLocation(Frame.TransformPosition(FVector(400,0,0)));
            if(InBox(TEXT("yacht/")+FString::FromInt(Yacht->Index),Frame,FVector(2190,688,578),493))return true;
        }
    }
    const auto Coord=M->PlayerCoord();const FVector Local=M->ToLocal(Coord,Eye);
    if(EWOuterWater::Contains(M->Descriptor(),Coord) && M->ReadyAt(Coord))
    {
        const double Top=EWOuterWater::WaterHeight(M->Descriptor(),Coord);
        if(InBox(TEXT("outer-water"),FTransform(M->ToRender(Coord,FVector(6400,6400,Top))),FVector(6400,6400,0),-900))return true;
    }
    if(EW::IsWaterCityDistrict(M->Descriptor(),Coord))
    {
        // Water-city polygons use reference-city coordinates, spanning chunk edges.
        const FVector P=Local+FVector(Coord.X*EW::ChunkSize,Coord.Y*EW::ChunkSize,0);
        for(const auto& B:EW::GetWaterCityPlan().Bodies)
        {
            if(P.Z>=B.WaterZ || P.Z<=B.WaterZ-B.Depth || B.Polygon.Num()<3)continue;
            bool Inside=false;FVector2D Lo(DBL_MAX,DBL_MAX),Hi(-DBL_MAX,-DBL_MAX);
            for(int32 I=0,J=B.Polygon.Num()-1;I<B.Polygon.Num();J=I++)
            {
                const auto A=B.Polygon[I],C=B.Polygon[J];Lo.X=FMath::Min(Lo.X,A.X);Lo.Y=FMath::Min(Lo.Y,A.Y);Hi.X=FMath::Max(Hi.X,A.X);Hi.Y=FMath::Max(Hi.Y,A.Y);
                if((A.Y>P.Y)!=(C.Y>P.Y) && P.X<(C.X-A.X)*(P.Y-A.Y)/(C.Y-A.Y)+A.X)Inside=!Inside;
            }
            if(Inside && InBox(TEXT("water-city/")+B.Id,FTransform(M->ToRender({0,0},FVector((Lo.X+Hi.X)*.5,(Lo.Y+Hi.Y)*.5,B.WaterZ))),FVector((Hi.X-Lo.X)*.5,(Hi.Y-Lo.Y)*.5,0),-B.Depth))return true;
        }
    }
    return false;
}
void AEWWaterView::Tick(float Delta)
{
    Super::Tick(Delta);const auto* G=GetGameInstance<UEWGameInstance>();const auto* M=G?G->Manager.Get():nullptr;
    auto* PC=UGameplayStatics::GetPlayerController(this,0);auto* Camera=PC?PC->PlayerCameraManager.Get():nullptr;
    if(!M || !Camera)return;
    if(!bLoaded && M->ReadyCount()>8){Sites=EWPoolPlan::All();bLoaded=true;}
    const FVector Eye=Camera->GetCameraLocation();FEWWaterSample Found;
    const bool Under=Sample(Eye,Found);Current=Under?Found:FEWWaterSample();
    const float U=Under?FMath::Clamp(float(Found.Depth/8.),0.f,1.f):0.f;Weight=U*U*(3-2*U);
    Optical->BlendWeight=Material?Weight:0;
    if(Material && Under)
    {
        const FRotationMatrix Rotation(Camera->GetCameraRotation());const auto Q=Found.Frame.GetRotation();
        auto V=[&](const TCHAR* Key,const FVector& Value){Material->SetVectorParameterValue(Key,FLinearColor(Value.X,Value.Y,Value.Z,0));};
        V(TEXT("Forward"),Rotation.GetScaledAxis(EAxis::X));V(TEXT("Right"),Rotation.GetScaledAxis(EAxis::Y));V(TEXT("Up"),Rotation.GetScaledAxis(EAxis::Z));
        V(TEXT("WaterRight"),Q.GetAxisX());V(TEXT("WaterForward"),Q.GetAxisY());V(TEXT("LocalEye"),Found.Frame.InverseTransformPositionNoScale(Eye));V(TEXT("Extent"),Found.Extent);
        int32 Width=1600,Height=900;PC->GetViewportSize(Width,Height);
        const double Tan=FMath::Tan(FMath::DegreesToRadians(Camera->GetFOVAngle()*.5));
        Material->SetVectorParameterValue(TEXT("Lens"),FLinearColor(Tan,Tan*FMath::Max(1,Height)/FMath::Max(1,Width),Found.Bottom,0));
    }
    const EWPoolPlan::Pool* Nearest=nullptr;double Distance=3000*3000;
    if(M->Descriptor().Seed==EW::WorldDescriptor::ReferenceWorld().Seed)for(const auto& P:Sites)
    {
        if(!M->ReadyAt(P.Coord))continue;const double D=FVector::DistSquared(Eye,M->ToRender(P.Coord,P.Frame.TransformPosition(FVector(0,0,140))));
        if(D<Distance){Distance=D;Nearest=&P;}
    }
    const double Night=G->DayCycle?G->DayCycle->StreetLightScale():0;
    int32 I=0;for(double Y:{-410.,410.})for(double X:{-500.,0.,500.})
    {
        auto* L=Lamps[I++].Get();const bool Enabled=Nearest && Night>.01;
        if(Enabled){L->SetWorldLocation(M->ToRender(Nearest->Coord,Nearest->Frame.TransformPosition(FVector(X,Y,92))));L->SetIntensity(110000*Night);}
        L->SetVisibility(Enabled);
    }
}
bool AEWWaterView::VisitPool(int32 Index)
{
    if(!bLoaded){Sites=EWPoolPlan::All();bLoaded=true;}
    if(!Sites.IsValidIndex(Index))return false;
    if(auto* G=GetGameInstance<UEWGameInstance>()){G->Visit(EWPoolPlan::Arrival(Sites[Index]));return true;}return false;
}
bool AEWWaterView::PreviewPool(int32 Index,int32 View,double Hour)
{
#if WITH_EDITOR
    if(!GIsEditor || !Sites.IsValidIndex(Index) || !FMath::IsFinite(Hour))return false;
    auto* G=GetGameInstance<UEWGameInstance>();auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !G->Manager || !PC || !G->Manager->ReadyAt(Sites[Index].Coord))return false;
    const auto& P=Sites[Index];FVector Eye,Aim;
    switch(View)
    {
    case 0:Eye=FVector(-1300,1200,850);Aim=FVector(0,0,130);break;
    case 1:Eye=FVector(-160,-80,165);Aim=FVector(660,40,135);break;
    case 2:Eye=FVector(-160,-80,260);Aim=FVector(660,40,190);break;
    case 3:Eye=FVector(-160,-80,165);Aim=FVector(240,0,510);break;
    default:PC->SetViewTarget(UGameplayStatics::GetPlayerPawn(this,0));return true;
    }
    if(!PreviewCamera)PreviewCamera=GetWorld()->SpawnActor<ACameraActor>();
    Eye=G->Manager->ToRender(P.Coord,P.Frame.TransformPosition(Eye));Aim=G->Manager->ToRender(P.Coord,P.Frame.TransformPosition(Aim));
    PreviewCamera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());PreviewCamera->GetCameraComponent()->SetFieldOfView(85);
    PC->SetViewTarget(PreviewCamera);if(G->DayCycle)G->DayCycle->SetAuditHour(Hour);G->SetMenu(EEWMenu::None);return true;
#else
    return false;
#endif
}
TSharedRef<FJsonObject> AEWWaterView::Evidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("material_loaded"),Material!=nullptr);O->SetNumberField(TEXT("pool_count"),Sites.Num());
    O->SetNumberField(TEXT("weight"),Weight);O->SetStringField(TEXT("water_body"),Current.Id);O->SetNumberField(TEXT("camera_depth_cm"),Current.Depth);
    TArray<TSharedPtr<FJsonValue>> A;for(const auto& P:Sites)
    {auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("name"),P.Name);V->SetStringField(TEXT("coord"),P.Coord.Text());V->SetNumberField(TEXT("column"),P.Column);V->SetNumberField(TEXT("height"),P.Frame.GetLocation().Z);A.Add(MakeShared<FJsonValueObject>(V));}
    O->SetArrayField(TEXT("pools"),A);return O;
}
FString AEWWaterView::WaterEvidence() const
{FString Text;FJsonSerializer::Serialize(Evidence(),TJsonWriterFactory<>::Create(&Text));return Text;}
bool AEWWaterView::PreviewOptics(int32 Mode)
{
#if WITH_EDITOR
    if(GIsEditor && Material && Mode>=0 && Mode<=3){Material->SetScalarParameterValue(TEXT("DebugMode"),Mode);return true;}
#endif
    return false;
}
