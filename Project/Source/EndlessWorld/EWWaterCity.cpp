#include "EWWaterCity.h"
#include "EWSky92Plan.h"
#include "EWOuterWater.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWWaterCityPlan.h"
#include "EWWaterCityGardenPlan.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"

namespace EW
{
namespace
{
using Object=TSharedPtr<FJsonObject>;
FVector Vector(const Object& O,const TCHAR* Key)
{
    const auto& A=O->GetArrayField(Key);
    return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());
}
bool TouchesChunk(FVector A,FVector B,double Margin,ChunkCoord C)
{
    return FMath::Max(A.X,B.X)+Margin>=C.X*ChunkSize && FMath::Min(A.X,B.X)-Margin<=(C.X+1)*ChunkSize &&
           FMath::Max(A.Y,B.Y)+Margin>=C.Y*ChunkSize && FMath::Min(A.Y,B.Y)-Margin<=(C.Y+1)*ChunkSize;
}
WaterCityPlan ReadPlan()
{
    WaterCityPlan P;Object Root;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(WaterCityData::GetJson()),Root) || !Root.IsValid() ||
        Root->GetStringField(TEXT("format"))!=TEXT("water-city-plan-v1") || !Root->GetBoolField(TEXT("success")))
    {P.Error=TEXT("Water city source layout is invalid");return P;}
    P.Revision=Root->GetIntegerField(TEXT("revision"));P.BaselineSHA=Root->GetStringField(TEXT("baseline_sha256"));
    for(const auto& Pair:Root->GetObjectField(TEXT("baseline_digests"))->Values)P.BaselineDigests.Add(FString(Pair.Key),Pair.Value->AsString());
    for(const auto& Value:Root->GetArrayField(TEXT("bodies")))
    {
        const auto O=Value->AsObject();WaterCityBody B;
        B.Id=O->GetStringField(TEXT("id"));B.Group=O->GetStringField(TEXT("group"));
        B.WaterZ=O->GetNumberField(TEXT("water_z"));B.Depth=O->GetNumberField(TEXT("depth"));
        for(const auto& Point:O->GetArrayField(TEXT("polygon")))
        {const auto& XY=Point->AsArray();B.Polygon.Add(FVector2D(XY[0]->AsNumber(),XY[1]->AsNumber()));}
        const auto& Flow=O->GetArrayField(TEXT("flow"));B.Flow=FVector2D(Flow[0]->AsNumber(),Flow[1]->AsNumber());P.Bodies.Add(MoveTemp(B));
    }
    for(const auto& Value:Root->GetArrayField(TEXT("banks")))
    {
        const auto O=Value->AsObject();WaterCityBank B;B.A=Vector(O,TEXT("a"));B.B=Vector(O,TEXT("b"));
        B.Width=O->GetNumberField(TEXT("width"));B.Top=O->GetNumberField(TEXT("top_z"));B.Bottom=O->GetNumberField(TEXT("bottom_z"));P.Banks.Add(B);
    }
    for(const auto& Value:Root->GetArrayField(TEXT("falls")))
    {
        const auto O=Value->AsObject();WaterCityFall F;F.Id=O->GetStringField(TEXT("id"));F.Group=O->GetStringField(TEXT("group"));
        F.X0=O->GetNumberField(TEXT("x0"));F.X1=O->GetNumberField(TEXT("x1"));F.StartY=O->GetNumberField(TEXT("start_y"));
        F.EndY=O->GetNumberField(TEXT("end_y"));F.Top=O->GetNumberField(TEXT("top_z"));F.Bottom=O->GetNumberField(TEXT("bottom_z"));P.Falls.Add(F);
    }
    for(const auto& Value:Root->GetArrayField(TEXT("walks")))
    {
        const auto O=Value->AsObject();WaterCityWalk W;W.Id=O->GetStringField(TEXT("id"));W.A=Vector(O,TEXT("a"));W.B=Vector(O,TEXT("b"));
        W.Width=O->GetNumberField(TEXT("width"));W.Thickness=O->GetNumberField(TEXT("thickness"));W.EndPad=O->GetNumberField(TEXT("end_pad"));
        W.bAccess=O->GetBoolField(TEXT("access"));P.Walks.Add(W);
    }
    for(const auto& Value:Root->GetArrayField(TEXT("walk_rails")))
    {
        const auto O=Value->AsObject();WaterCityRail R;R.A=Vector(O,TEXT("a"));R.B=Vector(O,TEXT("b"));
        R.Height=O->GetNumberField(TEXT("height"));R.Width=O->GetNumberField(TEXT("width"));P.Rails.Add(R);
    }
    for(const auto& Value:Root->GetArrayField(TEXT("supports")))
    {
        const auto O=Value->AsObject();WaterCitySupport S;S.X=O->GetNumberField(TEXT("x"));S.Y=O->GetNumberField(TEXT("y"));
        S.Bottom=O->GetNumberField(TEXT("bottom_z"));S.Top=O->GetNumberField(TEXT("top_z"));
        S.HalfWidth=O->GetNumberField(TEXT("half_width"));S.HalfDepth=O->GetNumberField(TEXT("half_depth"));P.Supports.Add(S);
    }
    for(const auto& Value:Root->GetArrayField(TEXT("views")))
    {const auto O=Value->AsObject();WaterCityView V;V.Id=O->GetStringField(TEXT("id"));V.Eye=Vector(O,TEXT("eye"));V.Target=Vector(O,TEXT("target"));P.Views.Add(V);}
    P.bValid=P.Revision==1 && P.Bodies.Num()==11 && P.Falls.Num()==6 && P.Walks.Num()==16 && P.Supports.Num()==14;
    if(!P.bValid)P.Error=TEXT("Water city source layout counts changed without an audit revision");
    return P;
}
void AddBox(ChunkRecipe& R,FVector Global,FVector Extent,bool Floor=false,const FQuat& Rotation=FQuat::Identity)
{
    const FVector Horizontal(FMath::Abs(Rotation.GetAxisX().X)*Extent.X+FMath::Abs(Rotation.GetAxisY().X)*Extent.Y,
        FMath::Abs(Rotation.GetAxisX().Y)*Extent.X+FMath::Abs(Rotation.GetAxisY().Y)*Extent.Y,0);
    if(!TouchesChunk(Global-Horizontal,Global+Horizontal,2,R.Coord))return;
    const FVector Origin(R.Coord.X*ChunkSize,R.Coord.Y*ChunkSize,0);
    R.Colliders.Add({FTransform(Rotation,Global-Origin),Extent,Floor});
}
void AddBeam(ChunkRecipe& R,FVector A,FVector B,double Width,double Height,bool Floor=false,double EndPad=1)
{
    const FVector Delta=B-A;if(Delta.Size()<1)return;
    const FQuat Q=FRotationMatrix::MakeFromXZ(Delta.GetSafeNormal(),FVector::UpVector).ToQuat();
    AddBox(R,(A+B)*.5+Q.GetUpVector()*(Floor?-Height*.5:Height*.5),FVector(Delta.Size()*.5+EndPad,Width*.5,Height*.5),Floor,Q);
}
}

const WaterCityPlan& GetWaterCityPlan()
{
    static const WaterCityPlan Plan=ReadPlan();return Plan;
}
bool IsWaterCityDistrict(const WorldDescriptor& World,ChunkCoord Coord)
{
    return World.Seed==WorldDescriptor::ReferenceWorld().Seed && Coord.X==0 && Coord.Y>=-1 && Coord.Y<=1;
}
TArray<PathSegment> WaterCityAccessPaths(const WorldDescriptor& World,ChunkCoord Coord,bool bEndPads)
{
    TArray<PathSegment> Result;if(!IsWaterCityDistrict(World,Coord))return Result;
    const auto& P=GetWaterCityPlan();if(!P.bValid)return Result;
    const FVector Origin(Coord.X*ChunkSize,Coord.Y*ChunkSize,0);
    for(const auto& W:P.Walks)if(TouchesChunk(W.A,W.B,W.Width,Coord))
    {const FVector Extra=(W.B-W.A).GetSafeNormal()*(bEndPads?W.EndPad:0.);Result.Add({W.A-Extra-Origin,W.B+Extra-Origin,W.Width});}
    return Result;
}
void BuildWaterCity(ChunkRecipe& R)
{
    if(!IsWaterCityDistrict(R.World,R.Coord))return;
    const auto& P=GetWaterCityPlan();
    if(!P.bValid){R.bFallback=true;UE_LOG(LogTemp,Error,TEXT("EW_WATER_CITY_PLAN_INVALID %s"),*P.Error);return;}
    R.bWaterCity=true;R.WaterPaths=WaterCityAccessPaths(R.World,R.Coord);
    const FString Group=R.Coord.Y==0?TEXT("Central"):R.Coord.Y>0?TEXT("North"):TEXT("South");
    for(const TCHAR* Suffix:{TEXT("Stone"),TEXT("Water"),TEXT("Foam")})
        R.Parts.Add({FName(*(TEXT("WaterCity")+Group+Suffix)),FTransform::Identity,0});
    if(R.Coord.Y!=0)R.Parts.Add({FName(*(TEXT("WaterCity")+Group+TEXT("Fall"))),FTransform::Identity,0});
    if(R.Coord.Y==0)R.Parts.Add({TEXT("WaterCityWalks"),FTransform::Identity,0});
    // Collision is duplicated only across intersected chunk edges. A bookmark
    // in either chunk can then resolve to the same added bridge floor.
    for(const auto& W:P.Walks)AddBeam(R,W.A,W.B,W.Width,W.Thickness,true,W.EndPad);
    for(const auto& Rail:P.Rails)AddBeam(R,Rail.A,Rail.B,Rail.Width,Rail.Height);
    for(const auto& B:P.Bodies)
    {
        FVector2D Lo(DBL_MAX,DBL_MAX),Hi(-DBL_MAX,-DBL_MAX);
        for(const auto& XY:B.Polygon){Lo.X=FMath::Min(Lo.X,XY.X);Lo.Y=FMath::Min(Lo.Y,XY.Y);Hi.X=FMath::Max(Hi.X,XY.X);Hi.Y=FMath::Max(Hi.Y,XY.Y);}
        AddBox(R,FVector((Lo.X+Hi.X)*.5,(Lo.Y+Hi.Y)*.5,B.WaterZ-B.Depth-27.5),FVector((Hi.X-Lo.X)*.5,(Hi.Y-Lo.Y)*.5,27.5));
    }
    for(const auto& B:P.Banks)
    {FVector A=B.A,B0=B.B;A.Z=B0.Z=B.Bottom;AddBeam(R,A,B0,B.Width,B.Top-B.Bottom);}
    for(const auto& F:P.Falls)
        AddBox(R,FVector((F.X0+F.X1)*.5,F.StartY+110,F.Bottom-100),FVector((F.X1-F.X0)*.5,70,160));
    for(const auto& S:P.Supports)AddBox(R,FVector(S.X,S.Y,(S.Bottom+S.Top)*.5),FVector(S.HalfWidth,S.HalfDepth,(S.Top-S.Bottom)*.5));
    for(const auto& S:WaterGardenData::Solids())AddBox(R,S.Centre,S.Extent,true);
    const FVector GardenOrigin(R.Coord.X*ChunkSize,R.Coord.Y*ChunkSize,0);
    for(const auto& Plant:WaterGardenData::Plants())
    {
        // Each supported planting belongs to exactly one streamed district.
        if(FMath::FloorToInt(Plant.Position.X/ChunkSize)!=R.Coord.X || FMath::FloorToInt(Plant.Position.Y/ChunkSize)!=R.Coord.Y)continue;
        R.Parts.Add({FName(Plant.Mesh),FTransform(FRotator(0,Plant.Yaw,0),Plant.Position-GardenOrigin,Plant.Scale),1});
    }
}
}

AEWWaterCity::AEWWaterCity()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=1.f/30.f;
    Root=CreateDefaultSubobject<USceneComponent>(TEXT("WaterCityRoot"));SetRootComponent(Root);
}
void AEWWaterCity::AppendAssetPaths(TArray<FSoftObjectPath>& Paths)
{
    Paths.AddUnique(FSoftObjectPath(TEXT("/Game/EndlessWorld/Kit/SM_WaterCityDrop.SM_WaterCityDrop")));
}
bool AEWWaterCity::Initialize(EW::ChunkCoord Coord)
{
    const auto& Plan=EW::GetWaterCityPlan();if(!Plan.bValid)return false;
    const FString Group=Coord.Y>0?TEXT("North"):Coord.Y<0?TEXT("South"):TEXT("Central");
    const FVector Origin(Coord.X*EW::ChunkSize,Coord.Y*EW::ChunkSize,0);
    for(auto F:Plan.Falls)if(F.Group==Group)
    {F.X0-=Origin.X;F.X1-=Origin.X;F.StartY-=Origin.Y;F.EndY-=Origin.Y;LocalFalls.Add(F);}
    if(const auto* G=GetGameInstance<UEWGameInstance>();G && G->Manager && EWOuterWater::Contains(G->Manager->Descriptor(),Coord))
    {LocalFalls=EWOuterWater::Falls(G->Manager->Descriptor(),Coord);EWSky92Plan::Clouds(Root,G->Manager->Descriptor(),Coord);}
    if(LocalFalls.IsEmpty()){SetActorTickEnabled(false);return true;}
    UStaticMesh* Mesh=Cast<UStaticMesh>(FSoftObjectPath(TEXT("/Game/EndlessWorld/Kit/SM_WaterCityDrop.SM_WaterCityDrop")).ResolveObject());
    if(!Mesh)return false;
    Spray=NewObject<UInstancedStaticMeshComponent>(this);Spray->SetupAttachment(Root);Spray->SetStaticMesh(Mesh);
    Spray->SetMobility(EComponentMobility::Movable);Spray->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Spray->SetCanEverAffectNavigation(false);Spray->SetCastShadow(false);Spray->RegisterComponent();
    for(int32 I=0;I<LocalFalls.Num()*12;++I)Spray->AddInstance(FTransform(FVector::ZeroVector));
    // One band-limited loop per fall. The underflow callback owns only immutable
    // PCM, not an actor pointer; actor teardown stops and releases every voice.
    for(int32 Index=0;Index<LocalFalls.Num();++Index)
    {
        const auto& F=LocalFalls[Index];auto Data=MakeShared<TArray<uint8>,ESPMode::ThreadSafe>();
        constexpr int32 Rate=24000,Count=Rate*6;Data->SetNumUninitialized(Count*sizeof(int16));
        FRandomStream Random(80731+Index*331+int32(Coord.Y)*71);double Slow=0,Medium=0;
        for(int32 I=0;I<Count;++I)
        {
            const double Noise=Random.FRand()*2-1;Slow+=.022*(Noise-Slow);Medium+=.23*(Noise-Medium);
            const double T=double(I)/Rate;
            const double Swell=.78+.13*FMath::Sin(T*PI/3)+.06*FMath::Sin(T*2*PI/3+Index);
            const double Sample=FMath::Clamp((Slow*1.9+Medium*.48)*Swell,-.92,.92);
            reinterpret_cast<int16*>(Data->GetData())[I]=int16(Sample*17500);
        }
        auto* Wave=NewObject<USoundWaveProcedural>(this);Wave->SetSampleRate(Rate);Wave->NumChannels=1;
        Wave->Duration=INDEFINITELY_LOOPING_DURATION;Wave->bLooping=false;Wave->SoundGroup=SOUNDGROUP_Default;
        Wave->OnSoundWaveProceduralUnderflow=FOnSoundWaveProceduralUnderflow::CreateLambda([Data](USoundWaveProcedural* Underflow,int32)
        {if(Underflow && Underflow->GetAvailableAudioByteCount()<24000)Underflow->QueueAudio(Data->GetData(),Data->Num());});
        Wave->QueueAudio(Data->GetData(),Data->Num());Waves.Add(Wave);
        auto* Audio=NewObject<UAudioComponent>(this);Audio->SetupAttachment(Root);Audio->SetRelativeLocation(FVector((F.X0+F.X1)*.5,F.EndY,F.Bottom+160));
        Audio->bAutoActivate=false;Audio->bOverrideAttenuation=true;Audio->AttenuationOverrides.bAttenuate=true;
        Audio->AttenuationOverrides.bSpatialize=true;Audio->AttenuationOverrides.AttenuationShape=EAttenuationShape::Sphere;
        Audio->AttenuationOverrides.AttenuationShapeExtents=FVector(650,0,0);Audio->AttenuationOverrides.FalloffDistance=5200;
        Audio->AttenuationOverrides.bEnableOcclusion=true;Audio->AttenuationOverrides.OcclusionLowPassFilterFrequency=1400;
        Audio->AttenuationOverrides.OcclusionVolumeAttenuation=.32;Audio->SetVolumeMultiplier(.32);
        Audio->SetSound(Wave);Audio->RegisterComponent();Audio->Play();Sounds.Add(Audio);
    }
    Tick(0);return true;
}
void AEWWaterCity::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(!Spray || LocalFalls.IsEmpty())return;
    const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
    const FVector Eye=Camera?Camera->GetCameraLocation():FVector::ZeroVector;
    bool AnyNear=false;
    for(const auto& F:LocalFalls)
        AnyNear|=FVector::DistSquared(Eye,GetActorTransform().TransformPosition(FVector((F.X0+F.X1)*.5,F.EndY,F.Bottom+100)))<FMath::Square(4200.);
    if(!AnyNear){Spray->SetVisibility(false,true);return;}
    const double Time=GetWorld()->GetTimeSeconds();int32 Instance=0;bool AnyVisible=false;
    for(int32 FIndex=0;FIndex<LocalFalls.Num();++FIndex)
    {
        const auto& F=LocalFalls[FIndex];const FVector Centre((F.X0+F.X1)*.5,F.EndY,F.Bottom+100);
        const bool Near=FVector::DistSquared(Eye,GetActorTransform().TransformPosition(Centre))<FMath::Square(4200.);
        AnyVisible|=Near;
        for(int32 I=0;I<12;++I,++Instance)
        {
            const double Phase=FMath::Frac(Time/(.72+.055*(I%5))+I*.61803398875+FIndex*.37);
            const double Across=FMath::Frac(I*.38196601125+FIndex*.23);
            const FVector P(FMath::Lerp(F.X0,F.X1,Across)+FMath::Sin(I*1.9)*Phase*65,
                F.EndY-30-Phase*(90+(I%4)*45),F.Bottom+15+FMath::Sin(Phase*PI)*(100+(I%5)*32));
            const double Scale=Near?(.50+.08*(I%6))*FMath::Sin(Phase*PI):.0001;
            const FTransform T(FRotator(15*FMath::Cos(Phase*PI),I*47.,0),P,FVector(Scale,Scale,Scale*(1.5+Phase)));
            Spray->UpdateInstanceTransform(Instance,T,false,Instance==LocalFalls.Num()*12-1,true);
        }
    }
    Spray->SetVisibility(AnyVisible,true);
}
int32 AEWWaterCity::SprayInstanceCount() const{return Spray?Spray->GetInstanceCount():0;}
int32 AEWWaterCity::ActiveSoundCount() const
{int32 Count=0;for(const auto& A:Sounds)if(A && A->IsPlaying())++Count;return Count;}
void AEWWaterCity::EndPlay(const EEndPlayReason::Type Reason)
{
    for(const auto& A:Sounds)if(A)A->Stop();Super::EndPlay(Reason);
}
