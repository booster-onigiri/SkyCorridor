#include "EWWaterCityAuditCommandlet.h"
#include "EWWorld.h"
#include "EWWaterCity.h"
#include "EWWaterCityPlan.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#if WITH_EDITOR
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#endif

namespace
{
using Object=TSharedPtr<FJsonObject>;
TArray<TSharedPtr<FJsonValue>> V(FVector P)
{ return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)}; }
Object T(const FTransform& Transform)
{
    auto O=MakeShared<FJsonObject>();O->SetArrayField(TEXT("position"),V(Transform.GetLocation()));
    O->SetArrayField(TEXT("scale"),V(Transform.GetScale3D()));
    const auto R=Transform.Rotator();O->SetArrayField(TEXT("rotation"),V(FVector(R.Roll,R.Pitch,R.Yaw)));return O;
}
void Add(TArray<TSharedPtr<FJsonValue>>& Array,Object O){Array.Add(MakeShared<FJsonValueObject>(O));}
FVector Point(const Object& O,const TCHAR* Field)
{const auto& A=O->GetArrayField(Field);return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());}
FString ShapeKey(const Object& O)
{
    FString Key;
    for(const TCHAR* Field:{TEXT("mesh"),TEXT("id"),TEXT("label")})
        if(O->HasField(Field))Key+=FString(Field)+TEXT(":")+O->GetStringField(Field)+TEXT(";");
    for(const TCHAR* Field:{TEXT("position"),TEXT("scale"),TEXT("rotation"),TEXT("extent"),TEXT("a"),TEXT("b"),TEXT("cabin"),TEXT("outward"),TEXT("landing")})
        if(O->HasField(Field))for(const auto& Value:O->GetArrayField(Field))Key+=FString::Printf(TEXT("%lld,"),int64(FMath::RoundToDouble(Value->AsNumber()*1000)));
    for(const TCHAR* Field:{TEXT("width"),TEXT("height"),TEXT("column"),TEXT("detail")})
        if(O->HasField(Field))Key+=FString(Field)+FString::Printf(TEXT("=%lld;"),int64(FMath::RoundToDouble(O->GetNumberField(Field)*1000)));
    for(const TCHAR* Field:{TEXT("floor"),TEXT("roof")})if(O->HasField(Field))Key+=FString(Field)+(O->GetBoolField(Field)?TEXT("=1;"):TEXT("=0;"));
    if(O->HasField(TEXT("stops")))for(const auto& Stop:O->GetArrayField(TEXT("stops")))Key+=TEXT("STOP{")+ShapeKey(Stop->AsObject())+TEXT("}");
    return Key;
}
bool EqualShapes(const TArray<TSharedPtr<FJsonValue>>& A,const TArray<TSharedPtr<FJsonValue>>& B)
{
    if(A.Num()!=B.Num())return false;
    TMap<FString,int32> Counts;for(const auto& V0:B)++Counts.FindOrAdd(ShapeKey(V0->AsObject()));
    for(const auto& V0:A){int32* Count=Counts.Find(ShapeKey(V0->AsObject()));if(!Count || --*Count<0)return false;}
    return true;
}
struct AuditCollider{EW::Collider Box;EW::ChunkCoord Coord;int32 Index=0;};
bool Clear(FVector Global,const TArray<AuditCollider>& Boxes,FString* Hit=nullptr)
{
    for(const auto& Item:Boxes)
    {
        const auto& B=Item.Box;const FVector P=Global-FVector(Item.Coord.X*EW::ChunkSize,Item.Coord.Y*EW::ChunkSize,0);
        const FQuat Q=B.Transform.GetRotation();
        const double HalfZ=FMath::Abs(Q.GetAxisX().Z)*B.Extent.X+FMath::Abs(Q.GetAxisY().Z)*B.Extent.Y+FMath::Abs(Q.GetAxisZ().Z)*B.Extent.Z;
        if(FMath::Abs(P.Z-B.Transform.GetLocation().Z)>HalfZ+88)continue;
        const FVector Local=B.Transform.InverseTransformPosition(P);
        if(FMath::Abs(Local.X)<B.Extent.X+37.95 && FMath::Abs(Local.Y)<B.Extent.Y+37.95 && FMath::Abs(Local.Z)<B.Extent.Z+87.95)
        {if(Hit)*Hit=Item.Coord.Text()+FString::Printf(TEXT("/%d"),Item.Index);return false;}
    }
    return true;
}
Object Describe(const EW::ChunkRecipe& R)
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("coord"),R.Coord.Text());O->SetArrayField(TEXT("hub"),V(R.Hub));
    O->SetStringField(TEXT("digest"),R.Digest());TArray<TSharedPtr<FJsonValue>> Parts,Colliders,Paths,Lifts,Floors;
    for(const auto& P:R.Parts)
    { auto Item=T(P.Transform);Item->SetStringField(TEXT("mesh"),P.Mesh.ToString());Item->SetNumberField(TEXT("detail"),P.Detail);Add(Parts,Item); }
    for(const auto& C:R.Colliders)
    { auto Item=T(C.Transform);Item->SetArrayField(TEXT("extent"),V(C.Extent));Item->SetBoolField(TEXT("floor"),C.bWalkSurface);Add(Colliders,Item); }
    for(const auto& P:R.Paths)
    { auto Item=MakeShared<FJsonObject>();Item->SetArrayField(TEXT("a"),V(P.A));Item->SetArrayField(TEXT("b"),V(P.B));Item->SetNumberField(TEXT("width"),P.Width);Add(Paths,Item); }
    for(const auto& L:R.Lifts)
    {
        auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("id"),L.Id);Item->SetArrayField(TEXT("cabin"),V(L.Cabin));
        Item->SetArrayField(TEXT("outward"),V(L.Outward));TArray<TSharedPtr<FJsonValue>> Stops;
        for(const auto& S:L.Stops)
        { auto Stop=MakeShared<FJsonObject>();Stop->SetNumberField(TEXT("height"),S.Height);Stop->SetArrayField(TEXT("landing"),V(S.Landing));Stop->SetStringField(TEXT("label"),S.Label);Add(Stops,Stop); }
        Item->SetArrayField(TEXT("stops"),Stops);Add(Lifts,Item);
    }
    for(const auto& F:R.CityFloors)
    { auto Item=T(F.Transform);Item->SetNumberField(TEXT("column"),F.Column);Item->SetBoolField(TEXT("roof"),F.bRoof);Add(Floors,Item); }
    O->SetArrayField(TEXT("parts"),Parts);O->SetArrayField(TEXT("colliders"),Colliders);O->SetArrayField(TEXT("paths"),Paths);
    O->SetArrayField(TEXT("lifts"),Lifts);O->SetArrayField(TEXT("city_floors"),Floors);return O;
}
Object Palette(const TCHAR* Name)
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("mesh"),Name);
#if WITH_EDITOR
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/EndlessWorld/Kit/SM_%s.SM_%s"),Name,Name));
    const FMeshDescription* Description=Mesh?Mesh->GetMeshDescription(0):nullptr;
    if(!Description){O->SetStringField(TEXT("status"),TEXT("UNAVAILABLE"));return O;}
    FStaticMeshConstAttributes Attributes(*Description);const auto Colours=Attributes.GetVertexInstanceColors();
    const auto Names=Attributes.GetPolygonGroupMaterialSlotNames();
    struct Stat{int64 Count=0;FVector4f Min{1,1,1,1},Max{0,0,0,0};FVector4d Sum{0,0,0,0};};
    TMap<FName,Stat> Stats;
    for(const auto Triangle:Description->Triangles().GetElementIDs())
    {
        auto& S=Stats.FindOrAdd(Names[Description->GetTrianglePolygonGroup(Triangle)]);
        for(const auto Instance:Description->GetTriangleVertexInstances(Triangle))
        {
            const FVector4f Colour=Colours[Instance];++S.Count;
            for(int32 I=0;I<4;++I){S.Min[I]=FMath::Min(S.Min[I],Colour[I]);S.Max[I]=FMath::Max(S.Max[I],Colour[I]);S.Sum[I]+=Colour[I];}
        }
    }
    TArray<TSharedPtr<FJsonValue>> Groups;
    for(const auto& Pair:Stats)
    {
        auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("slot"),Pair.Key.ToString());Item->SetNumberField(TEXT("corners"),Pair.Value.Count);
        TArray<TSharedPtr<FJsonValue>> Min,Max,Mean;
        for(int32 I=0;I<4;++I)
        {Min.Add(MakeShared<FJsonValueNumber>(Pair.Value.Min[I]));Max.Add(MakeShared<FJsonValueNumber>(Pair.Value.Max[I]));Mean.Add(MakeShared<FJsonValueNumber>(Pair.Value.Sum[I]/FMath::Max<int64>(1,Pair.Value.Count)));}
        Item->SetArrayField(TEXT("min"),Min);Item->SetArrayField(TEXT("max"),Max);Item->SetArrayField(TEXT("mean"),Mean);Add(Groups,Item);
    }
    O->SetStringField(TEXT("status"),TEXT("LOD0_VERTEX_INSTANCE_COLOURS_READ"));O->SetArrayField(TEXT("groups"),Groups);
    O->SetNumberField(TEXT("vertex_instances"),Description->VertexInstances().Num());
#else
    O->SetStringField(TEXT("status"),TEXT("EDITOR_BUILD_REQUIRED"));
#endif
    return O;
}
}

UEWWaterCityAuditCommandlet::UEWWaterCityAuditCommandlet()
{ IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;ShowErrorCount=true; }
int32 UEWWaterCityAuditCommandlet::Main(const FString& Params)
{
    FString Output,BaselinePath,BaselineText;
    if(!FParse::Value(*Params,TEXT("EWReport="),Output) || IFileManager::Get().FileExists(*Output) ||
       !FParse::Value(*Params,TEXT("EWBaseline="),BaselinePath) || !FFileHelper::LoadFileToString(BaselineText,*BaselinePath))return 2;
    Output=FPaths::ConvertRelativePathToFull(Output);
    if(!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output),true))return 2;
    Object Baseline,PlanObject;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BaselineText),Baseline) || !Baseline.IsValid() ||
       Baseline->GetStringField(TEXT("format"))!=TEXT("water-city-existing-layout-v1") || !Baseline->GetBoolField(TEXT("success")) ||
       !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(EW::WaterCityData::GetJson()),PlanObject))return 2;
    auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("format"),TEXT("water-city-plan-audit-v1"));
    Report->SetStringField(TEXT("scope"),TEXT("Read-only authored plan, preserved old geometry, new walking clearances, optional imported LOD0; no asset or player-data writes"));
    Report->SetObjectField(TEXT("plan"),PlanObject);Report->SetStringField(TEXT("baseline"),FPaths::ConvertRelativePathToFull(BaselinePath));
    TArray<TSharedPtr<FJsonValue>> Districts,Palettes,Checks,Failures;bool Success=true;FString Error;
    auto Check=[&](const FString& Name,bool Pass)
    {auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),Name);O->SetBoolField(TEXT("pass"),Pass);Add(Checks,O);Success&=Pass;};
    auto Fail=[&](const FString& Name,FVector P,const FString& Hit)
    {if(Failures.Num()<80){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),Name);O->SetArrayField(TEXT("point"),V(P));O->SetStringField(TEXT("hit"),Hit);Add(Failures,O);}};
    const auto& Plan=EW::GetWaterCityPlan();Check(TEXT("authored_plan_valid"),Plan.bValid);
    Check(TEXT("generator_and_catalogue_versions_unchanged"),EW::GeneratorVersion==2 && EW::CatalogueVersion==1);
    TMap<FString,Object> Originals;
    for(const auto& D:Baseline->GetArrayField(TEXT("districts")))Originals.Add(D->AsObject()->GetStringField(TEXT("coord")),D->AsObject());
    TSet<FString> AllowedGuards;
    for(const auto& G:PlanObject->GetObjectField(TEXT("checks"))->GetArrayField(TEXT("expected_guard_openings")))
    {const auto& A=G->AsArray();AllowedGuards.Add(A[0]->AsString()+FString::Printf(TEXT("/%d"),int32(A[1]->AsNumber())));}
    TArray<AuditCollider> Before,After;
    for(const EW::ChunkCoord C:{EW::ChunkCoord{0,-1},EW::ChunkCoord{0,0},EW::ChunkCoord{0,1}})
    {
        const auto R=EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(),C);const auto Current=Describe(R);Add(Districts,Current);
        Check(C.Text()+TEXT("/recipe_valid"),R.Validate(Error)&&!R.bFallback&&R.bWaterCity&&R.Lifts.Num()==4);
        const Object* OriginalPtr=Originals.Find(C.Text());if(!OriginalPtr){Check(C.Text()+TEXT("/baseline_present"),false);continue;}
        const auto& Original=*OriginalPtr;
        Check(C.Text()+TEXT("/baseline_digest_matches_source"),Plan.BaselineDigests.FindRef(C.Text())==Original->GetStringField(TEXT("digest")));
        for(const TCHAR* Field:{TEXT("paths"),TEXT("lifts"),TEXT("city_floors")})
            Check(C.Text()+TEXT("/old_")+Field+TEXT("_preserved"),EqualShapes(Original->GetArrayField(Field),Current->GetArrayField(Field)));
        TArray<TSharedPtr<FJsonValue>> OldParts,NewParts;
        for(const auto& P0:Original->GetArrayField(TEXT("parts")))if(P0->AsObject()->GetStringField(TEXT("mesh"))!=TEXT("StoneRail"))OldParts.Add(P0);
        for(const auto& P0:Current->GetArrayField(TEXT("parts")))
        {const FString Name=P0->AsObject()->GetStringField(TEXT("mesh"));if(Name!=TEXT("StoneRail")&&!Name.StartsWith(TEXT("WaterCity")))NewParts.Add(P0);}
        Check(C.Text()+TEXT("/old_mesh_placements_preserved_except_connection_rails"),EqualShapes(OldParts,NewParts));
        TMap<FString,int32> NewBoxes;for(const auto& B:Current->GetArrayField(TEXT("colliders")))++NewBoxes.FindOrAdd(ShapeKey(B->AsObject()));
        bool CollisionPreserved=true;int32 Index=0;
        for(const auto& B:Original->GetArrayField(TEXT("colliders")))
        {
            const auto O=B->AsObject();const FVector Angles=Point(O,TEXT("rotation"));
            Before.Add({{FTransform(FRotator(Angles.Y,Angles.Z,Angles.X),Point(O,TEXT("position"))),Point(O,TEXT("extent")),O->GetBoolField(TEXT("floor"))},C,Index});
            if(!AllowedGuards.Contains(C.Text()+FString::Printf(TEXT("/%d"),Index)))
            {int32* Count=NewBoxes.Find(ShapeKey(O));if(!Count || --*Count<0)CollisionPreserved=false;}
            ++Index;
        }
        Check(C.Text()+TEXT("/all_other_old_colliders_preserved"),CollisionPreserved);
        for(int32 I=0;I<R.Colliders.Num();++I)After.Add({R.Colliders[I],C,I});
        Check(C.Text()+TEXT("/repeat_recipe_digest"),R.Digest()==EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(),C).Digest());
    }
    int32 NewPoints=0,NewBlocked=0;
    for(const auto& W:Plan.Walks)
    {
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist(W.A,W.B)/65.));
        for(int32 I=0;I<=Steps;++I)
        {
            const FVector Feet=FMath::Lerp(W.A,W.B,double(I)/Steps);FString Hit;++NewPoints;
            if(!Clear(Feet+FVector(0,0,88),After,&Hit)){++NewBlocked;Fail(W.Id,Feet,Hit);}
        }
    }
    Check(TEXT("all_new_walk_capsules_clear"),NewPoints>=374 && NewBlocked==0);
    int32 ProtectedPoints=0,ProtectedBlocked=0;
    for(const auto& Item:Before)
    {
        const auto& B=Item.Box;if(!B.bWalkSurface || B.Transform.GetRotation().GetUpVector().Z<.7)continue;
        const FVector Origin(Item.Coord.X*EW::ChunkSize,Item.Coord.Y*EW::ChunkSize,0);
        for(const FVector2D UV:{FVector2D(0,0),FVector2D(-.65,0),FVector2D(.65,0),FVector2D(0,-.65),FVector2D(0,.65)})
        {
            const FVector Feet=B.Transform.TransformPosition(FVector(UV.X*FMath::Max(0.,B.Extent.X-75),UV.Y*FMath::Max(0.,B.Extent.Y-75),B.Extent.Z))+Origin;
            const FVector Capsule=Feet+FVector(0,0,88);if(!Clear(Capsule,Before))continue;
            ++ProtectedPoints;FString Hit;
            if(!Clear(Capsule,After,&Hit)){++ProtectedBlocked;Fail(TEXT("old_floor_clearance"),Feet,Hit);}
        }
    }
    Check(TEXT("old_clear_floor_samples_remain_clear"),ProtectedPoints>1000 && ProtectedBlocked==0);
    auto Other=EW::WorldDescriptor::ReferenceWorld();Other.Seed=TEXT("00000000000000000000000000000001");
    Check(TEXT("other_world_not_modified"),!EW::IsWaterCityDistrict(Other,{0,0}));
    for(const auto C:{EW::ChunkCoord{-1,0},EW::ChunkCoord{1,0},EW::ChunkCoord{0,-2},EW::ChunkCoord{0,2}})
        Check(TEXT("outside_scope_")+C.Text(),!EW::IsWaterCityDistrict(EW::WorldDescriptor::ReferenceWorld(),C));
    if(FParse::Param(*Params,TEXT("EWAssets")))
    {
        for(const auto& Name:PlanObject->GetArrayField(TEXT("meshes")))
        {const FString MeshName=Name->AsString();auto O=Palette(*MeshName);Check(TEXT("asset_")+MeshName,O->GetStringField(TEXT("status"))==TEXT("LOD0_VERTEX_INSTANCE_COLOURS_READ"));Add(Palettes,O);}
        Report->SetStringField(TEXT("asset_status"),TEXT("IMPORTED_LOD0_CHECKED"));
    }
    else Report->SetStringField(TEXT("asset_status"),TEXT("NOT_CHECKED_before_import"));
    Report->SetNumberField(TEXT("new_route_points"),NewPoints);Report->SetNumberField(TEXT("new_route_blocked"),NewBlocked);
    Report->SetNumberField(TEXT("protected_floor_points"),ProtectedPoints);Report->SetNumberField(TEXT("protected_floor_blocked"),ProtectedBlocked);
    Report->SetArrayField(TEXT("districts"),Districts);Report->SetArrayField(TEXT("imported_palettes"),Palettes);
    Report->SetArrayField(TEXT("checks"),Checks);Report->SetArrayField(TEXT("failures"),Failures);Report->SetBoolField(TEXT("success"),Success);
    FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));
    if(!FFileHelper::SaveStringToFile(JSON,*Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return 2;
    UE_LOG(LogTemp,Display,TEXT("EW_WATER_CITY_LAYOUT_COMPLETE success=%d output=%s"),int32(Success),*Output);return Success?0:1;
}
