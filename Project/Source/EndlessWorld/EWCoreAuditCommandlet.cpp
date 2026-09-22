#include "EWCoreAuditCommandlet.h"
#include "EWWorld.h"
#include "EWSkyrailPlan.h"
#include "EWSaveStore.h"
#include "Async/ParallelFor.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#if WITH_EDITOR
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#endif

UEWCoreAuditCommandlet::UEWCoreAuditCommandlet()
{
    // Editor toolset dependencies in UE 5.8 require an initialized editor engine.
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true; ShowErrorCount = true;
}

int32 UEWCoreAuditCommandlet::Main(const FString& Params)
{
    const double Started = FPlatformTime::Seconds();
    FString Output = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Verification/core-audit.json"));
    FParse::Value(*Params, TEXT("EWAuditOutput="), Output);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output), true);
    auto Report = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Failures;
    auto Check = [&](bool OK, const FString& Name)
    {
        if (!OK) { Failures.Add(MakeShared<FJsonValueString>(Name)); UE_LOG(LogTemp, Error, TEXT("EW_AUDIT_FAIL %s"), *Name); }
    };
    if(FParse::Param(*Params,TEXT("EWSkyrailRouteAudit")))
    {
        const auto Result=EWSkyrailPlan::AuditRoute();FString Text;
        FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Text));
        FFileHelper::SaveStringToFile(Text,*Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        return Result->GetBoolField(TEXT("success"))?0:1;
    }
    if(FParse::Param(*Params,TEXT("EWAccessLayoutAudit")))
    {
        if(IFileManager::Get().FileExists(*Output))return 2;
        int32 Floors=0,Connected=0,Isolated=0,Bridges=0,Covered=0,Landings=0,Angled=0,MaxDegree=0;
        TArray<TSharedPtr<FJsonValue>> Districts;
        for(int32 Seed=0;Seed<4;++Seed)
        {
            const EW::WorldDescriptor W=Seed==0?EW::WorldDescriptor::ReferenceWorld():EW::WorldDescriptor{FString::Printf(TEXT("%032x"),Seed*731+43)};
            for(EW::ChunkCoord Centre:{EW::ChunkCoord{0,0},EW::ChunkCoord{1,0},EW::ChunkCoord{0,1},EW::ChunkCoord{-1,-1}})
            {
                const auto R=EW::GenerateChunk(W,Centre);
                Check(!R.bFallback,TEXT("access layout must not use fallback ")+Centre.Text());
                TArray<EW::Part> AllBridges;
                TArray<TPair<EW::CityWalkFloor,double>> NeighbourFloors;
                for(EW::ChunkCoord Offset:{EW::ChunkCoord{0,0},EW::ChunkCoord{-1,0},EW::ChunkCoord{0,-1},EW::ChunkCoord{1,0},EW::ChunkCoord{0,1}})
                {
                    const auto N=Offset==EW::ChunkCoord{0,0}?R:EW::GenerateChunk(W,Centre+Offset);
                    const FVector Shift(Offset.X*EW::ChunkSize,Offset.Y*EW::ChunkSize,0);
                    for(auto P:N.Parts)if(P.Mesh.ToString().StartsWith(TEXT("UrbanBridge_")) || P.Mesh.ToString().StartsWith(TEXT("UrbanPromenade_")))
                    {P.Transform.AddToTranslation(Shift);AllBridges.Add(P);}
                    for(auto F:N.CityFloors){F.Transform.AddToTranslation(Shift);NeighbourFloors.Add({F,N.Hub.Z});}
                }
                int32 LocalBridges=0,LocalCovered=0;
                for(const auto& P:R.Parts)
                {
                    const FString Name=P.Mesh.ToString();
                    if(Name.StartsWith(TEXT("UrbanBridge_")) || Name.StartsWith(TEXT("UrbanPromenade_")))
                    {++LocalBridges;if(Name.StartsWith(TEXT("UrbanPromenade_")))++LocalCovered;}
                }
                Bridges+=LocalBridges;Covered+=LocalCovered;
                for(const auto& F:R.CityFloors)
                {
                    ++Floors;const double Z=F.Transform.GetLocation().Z;
                    if(FMath::Abs(Z-R.Hub.Z)<1)continue;
                    const bool Possible=NeighbourFloors.ContainsByPredicate([&](const auto& Other)
                    {
                        const FVector P=Other.Key.Transform.GetLocation();
                        const double D=FVector::Dist2D(P,F.Transform.GetLocation());
                        return D>5200 && D<7000 && FMath::Abs(P.Z-Z)<46 && FMath::Abs(P.Z-Other.Value)>1;
                    });
                    int32 Degree=0;
                    for(const auto& B:AllBridges)for(double End:{-3200.,3200.})
                    {
                        const FVector P=F.Transform.InverseTransformPosition(B.Transform.TransformPosition(FVector(End,0,0)));
                        if(FMath::Abs(P.Z)<.1 && FMath::Max(FMath::Abs(P.X),FMath::Abs(P.Y))>2200 &&
                           FMath::Max(FMath::Abs(P.X),FMath::Abs(P.Y))<2350 && FMath::Min(FMath::Abs(P.X),FMath::Abs(P.Y))<800)++Degree;
                    }
                    if(Possible){++Connected;Check(Degree>0,FString::Printf(TEXT("unconnected floor seed=%d chunk=%s column=%d z=%.0f"),Seed,*Centre.Text(),F.Column,Z));}
                    else ++Isolated;
                    MaxDegree=FMath::Max(MaxDegree,Degree);
                    Check(Degree<=4,TEXT("excessive bridge degree"));
                }
                for(const auto& L:R.Lifts)for(const auto& S:L.Stops)
                {
                    ++Landings;const FVector Car(L.Cabin.X,L.Cabin.Y,S.Height);
                    const FVector Along=(S.Entry-Car).GetSafeNormal2D();
                    Check(FVector::DotProduct(Along,S.BoardingRotation.RotateVector(FVector(0,1,0)))>.99999,TEXT("door does not face actual gallery"));
                    Check(FVector::DotProduct(FVector::UpVector,S.BoardingRotation.RotateVector(FVector::UpVector))>.99999,TEXT("boarding frame must stay upright"));
                    Check(FVector::Dist(S.Threshold,Car+Along*200)<.01 && FVector::Dist(S.Landing,S.Threshold+Along*180)<.01,TEXT("door and landing disagree"));
                    if(!S.BoardingRotation.Equals(L.Rotation,.1))++Angled;
                    const FVector Safe=R.SafePosition(S.Landing+FVector(0,0,100));
                    Check(FMath::Abs(Safe.Z-S.Height-100)<1,TEXT("landing save resolves to another floor"));
                }
                auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("world"),W.Code());D->SetStringField(TEXT("coord"),Centre.Text());
                D->SetNumberField(TEXT("bridges"),LocalBridges);D->SetNumberField(TEXT("covered_bridges"),LocalCovered);
                D->SetNumberField(TEXT("public_floors"),R.CityFloors.Num());D->SetNumberField(TEXT("lowest_walk_surface"),R.LowestWalkSurface());
                Districts.Add(MakeShared<FJsonValueObject>(D));
            }
        }
        Check(Covered<double(Bridges)*.1,TEXT("covered bridges dominate the view"));
        Check(Angled>0,TEXT("setback landings were not exercised"));
        Report->SetBoolField(TEXT("success"),Failures.IsEmpty());Report->SetArrayField(TEXT("failures"),Failures);
        Report->SetArrayField(TEXT("districts"),Districts);Report->SetNumberField(TEXT("public_floors"),Floors);
        Report->SetNumberField(TEXT("connectable_floors"),Connected);Report->SetNumberField(TEXT("isolated_top_floors"),Isolated);
        Report->SetNumberField(TEXT("bridges"),Bridges);Report->SetNumberField(TEXT("covered_bridges"),Covered);
        Report->SetNumberField(TEXT("max_bridges_per_floor"),MaxDegree);Report->SetNumberField(TEXT("landings"),Landings);
        Report->SetNumberField(TEXT("angled_landings"),Angled);Report->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
        FString S;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&S));
        return FFileHelper::SaveStringToFile(S,*Output) && Failures.IsEmpty()?0:1;
    }
#if WITH_EDITOR
    if (FParse::Param(*Params,TEXT("EWPaletteAudit")))
    {
        if (IFileManager::Get().FileExists(*Output)) return 2;
        TArray<TSharedPtr<FJsonValue>> Meshes;
        for (const FString Name : {TEXT("SkyWard_3"),TEXT("CanopyWorld_0"),TEXT("CrystalWorld_0"),TEXT("Tree_Willow"),TEXT("Window")})
        {
            auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/EndlessWorld/Kit/SM_")+Name+TEXT(".SM_")+Name));
            if (!Mesh || !Mesh->GetMeshDescription(0)) return 2;
            const FMeshDescription& Desc=*Mesh->GetMeshDescription(0);
            FStaticMeshConstAttributes Attributes(Desc);
            const auto Colors=Attributes.GetVertexInstanceColors();
            const auto Names=Attributes.GetPolygonGroupMaterialSlotNames();
            struct Stats { FVector Sum=FVector::ZeroVector, Min=FVector(1), Max=FVector::ZeroVector; int64 Count=0; };
            TMap<FName,Stats> Groups;
            const int32 Stride=FMath::Max(1,Desc.Triangles().Num()/10000); int32 Index=0;
            for (const FTriangleID ID:Desc.Triangles().GetElementIDs())
            {
                if ((Index++%Stride)!=0) continue;
                auto& S=Groups.FindOrAdd(Names[Desc.GetTrianglePolygonGroup(ID)]);
                for (const FVertexInstanceID Vertex:Desc.GetTriangleVertexInstances(ID))
                {
                    const auto C=Colors[Vertex]; const FVector RGB(C.X,C.Y,C.Z);
                    S.Sum+=RGB; S.Min=S.Min.ComponentMin(RGB); S.Max=S.Max.ComponentMax(RGB); ++S.Count;
                }
            }
            auto M=MakeShared<FJsonObject>(); M->SetStringField(TEXT("mesh"),Name);
            TArray<TSharedPtr<FJsonValue>> Slots;
            for (const auto& Slot:Mesh->GetStaticMaterials())
            {
                auto S=MakeShared<FJsonObject>();
                S->SetStringField(TEXT("imported_name"),Slot.ImportedMaterialSlotName.ToString());
                S->SetStringField(TEXT("slot_name"),Slot.MaterialSlotName.ToString());
                S->SetStringField(TEXT("material"),GetPathNameSafe(Slot.MaterialInterface));
                Slots.Add(MakeShared<FJsonValueObject>(S));
            }
            M->SetArrayField(TEXT("assigned_materials"),Slots);
            auto Values=MakeShared<FJsonObject>();
            for (const auto& Pair:Groups)
            {
                auto V=MakeShared<FJsonObject>(); const auto& S=Pair.Value;
                V->SetNumberField(TEXT("samples"),S.Count); V->SetStringField(TEXT("mean_rgb"),(S.Sum/double(S.Count)).ToString());
                V->SetStringField(TEXT("min_rgb"),S.Min.ToString()); V->SetStringField(TEXT("max_rgb"),S.Max.ToString());
                Values->SetObjectField(Pair.Key.ToString(),V);
            }
            M->SetObjectField(TEXT("material_vertex_colors"),Values); Meshes.Add(MakeShared<FJsonValueObject>(M));
        }
        Report->SetArrayField(TEXT("meshes"),Meshes); FString Json;
        FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        return 0;
    }
#endif
    if(FParse::Param(*Params,TEXT("EWCitySafetyAudit")))
    {
        TArray<TSharedPtr<FJsonValue>> Examples;int32 Intrusions=0,Routes=0;
        for(int32 Seed=0;Seed<200;++Seed)
        {
            EW::WorldDescriptor W{EW::HashText(FString::Printf(TEXT("endless-city-clearance-%d"),Seed)).Left(32)};
            const auto R=EW::GenerateChunk(W,{0,0});
            for(int32 I=0;I<R.Paths.Num();++I)
            {
                const auto& Path=R.Paths[I];++Routes;
                const int32 Count=FMath::CeilToInt(FVector::Dist2D(Path.A,Path.B)/40.);
                bool Hit=false;FVector Point;
                for(int32 J=0;J<=Count && !Hit;++J)
                {
                    Point=FMath::Lerp(Path.A,Path.B,double(J)/FMath::Max(1,Count));
                    for(const auto& L:R.Lifts)
                    {
                        if(Point.Z>R.Hub.Z+125 || Point.Z+176<R.Hub.Z)continue;
                        const FVector P=L.Rotation.UnrotateVector(Point-L.Cabin);
                        // The car is open toward local +Y. Its three other
                        // real walls must stay out of every ground walk line.
                        if((FMath::Abs(P.Y+190)<50 && FMath::Abs(P.X)<228) ||
                           (FMath::Abs(FMath::Abs(P.X)-190)<50 && FMath::Abs(P.Y)<228))
                        {Hit=true;break;}
                    }
                }
                if(Hit)
                {
                    ++Intrusions;if(Examples.Num()<25){auto E=MakeShared<FJsonObject>();E->SetNumberField(TEXT("seed"),Seed);
                    E->SetNumberField(TEXT("path"),I);E->SetStringField(TEXT("point"),Point.ToString());E->SetStringField(TEXT("world"),W.Code());Examples.Add(MakeShared<FJsonValueObject>(E));}
                }
            }
        }
        Report->SetBoolField(TEXT("success"),Intrusions==0);Report->SetNumberField(TEXT("intruding_routes"),Intrusions);
        Report->SetNumberField(TEXT("worlds"),200);Report->SetNumberField(TEXT("routes"),Routes);Report->SetArrayField(TEXT("examples"),Examples);
        FString S;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&S));FFileHelper::SaveStringToFile(S,*Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        return Intrusions==0?0:1;
    }
    if(FParse::Param(*Params,TEXT("EWAccessDump")))
    {
        const auto R=EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(),{0,0});
        auto Vec=[](FVector V){return TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};};
        TArray<TSharedPtr<FJsonValue>> Boxes,Paths,Floors,Lifts;
        for(const auto& C:R.Colliders)
        {
            auto B=MakeShared<FJsonObject>();B->SetArrayField(TEXT("position"),Vec(C.Transform.GetLocation()));
            B->SetArrayField(TEXT("rotation"),Vec(C.Transform.Rotator().Euler()));B->SetArrayField(TEXT("extent"),Vec(C.Extent));
            B->SetBoolField(TEXT("floor"),C.bWalkSurface);Boxes.Add(MakeShared<FJsonValueObject>(B));
        }
        for(const auto& P:R.Paths){auto B=MakeShared<FJsonObject>();B->SetArrayField(TEXT("a"),Vec(P.A));B->SetArrayField(TEXT("b"),Vec(P.B));Paths.Add(MakeShared<FJsonValueObject>(B));}
        for(const auto& F:R.CityFloors){auto B=MakeShared<FJsonObject>();B->SetNumberField(TEXT("column"),F.Column);B->SetArrayField(TEXT("position"),Vec(F.Transform.GetLocation()));B->SetArrayField(TEXT("scale"),Vec(F.Transform.GetScale3D()));B->SetArrayField(TEXT("rotation"),Vec(F.Transform.Rotator().Euler()));Floors.Add(MakeShared<FJsonValueObject>(B));}
        for(const auto& L:R.Lifts)
        {
            auto B=MakeShared<FJsonObject>();B->SetStringField(TEXT("id"),L.Id);B->SetArrayField(TEXT("cabin"),Vec(L.Cabin));B->SetArrayField(TEXT("outward"),Vec(L.Outward));
            TArray<TSharedPtr<FJsonValue>> Stops;for(const auto& S:L.Stops){auto T=MakeShared<FJsonObject>();T->SetStringField(TEXT("label"),S.Label);T->SetArrayField(TEXT("landing"),Vec(S.Landing));Stops.Add(MakeShared<FJsonValueObject>(T));}
            B->SetArrayField(TEXT("stops"),Stops);Lifts.Add(MakeShared<FJsonValueObject>(B));
        }
        Report->SetArrayField(TEXT("colliders"),Boxes);Report->SetArrayField(TEXT("paths"),Paths);Report->SetArrayField(TEXT("floors"),Floors);Report->SetArrayField(TEXT("lifts"),Lifts);
        FString S;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&S));return FFileHelper::SaveStringToFile(S,*Output)?0:1;
    }
    FString WalkingReport;
    if (FParse::Value(*Params,TEXT("EWRouteEvidence="),WalkingReport))
    {
        // Read-only replay of the actual packaged game's movement evidence.
        // Recipe digests must match the ones produced by that running build.
        if (IFileManager::Get().FileExists(*Output)) return 2;
        FString SummaryText, LinesText; TSharedPtr<FJsonObject> Summary;
        if (!FFileHelper::LoadFileToString(SummaryText,*WalkingReport) ||
            !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SummaryText),Summary) ||
            !FFileHelper::LoadFileToString(LinesText,*FPaths::ChangeExtension(WalkingReport,TEXT("jsonl")))) return 2;
        EW::WorldDescriptor World; FString Error;
        if (!EW::WorldDescriptor::Parse(Summary->GetStringField(TEXT("world")),World,Error)) return 2;
        Check(Summary->GetBoolField(TEXT("success")),TEXT("source_runtime_completed"));
        TMap<EW::ChunkCoord,EW::ChunkRecipe> Recipes;
        auto Recipe=[&](EW::ChunkCoord C)->const EW::ChunkRecipe&
        { if (!Recipes.Contains(C)) Recipes.Add(C,EW::GenerateChunk(World,C)); return Recipes.FindChecked(C); };
        auto ParseCoord=[](const FString& Text,EW::ChunkCoord& C)
        { FString X,Y; return Text.Split(TEXT(","),&X,&Y) && LexTryParseString(C.X,*X) && LexTryParseString(C.Y,*Y) && C.Valid(); };
        double StartedWalking=-1, RegionTimes[3]={DBL_MAX,DBL_MAX,DBL_MAX};
        int32 RouteChecks=0, Samples=0;
        TSet<FString> FoundPlaces;
        TArray<TSharedPtr<FJsonValue>> Opportunities;
        TArray<FString> Lines; LinesText.ParseIntoArrayLines(Lines);
        for (const auto& Line:Lines)
        {
            TSharedPtr<FJsonObject> Event;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line),Event))
            { Check(false,TEXT("invalid_telemetry_json")); continue; }
            const FString Type=Event->GetStringField(TEXT("event"));
            if (Type==TEXT("route"))
            {
                FString CoordText,Digest; TArray<FString> Fields;
                Event->GetStringField(TEXT("detail")).ParseIntoArrayWS(Fields);
                for (const auto& Field:Fields)
                { if (Field.StartsWith(TEXT("chunk="))) CoordText=Field.Mid(6); if (Field.StartsWith(TEXT("digest="))) Digest=Field.Mid(7); }
                EW::ChunkCoord C;
                Check(ParseCoord(CoordText,C) && Recipe(C).Digest()==Digest,TEXT("running_build_recipe_matches_replay"));
                ++RouteChecks;
            }
            if (Type!=TEXT("sample") || Event->GetIntegerField(TEXT("phase"))!=4) continue;
            const double Elapsed=Event->GetNumberField(TEXT("elapsed"));
            if (StartedWalking<0) StartedWalking=Elapsed;
            if (!Event->GetBoolField(TEXT("on_ground"))) continue;
            EW::ChunkCoord C;
            if (!ParseCoord(Event->GetStringField(TEXT("coord")),C)) { Check(false,TEXT("invalid_sample_coord")); continue; }
            ++Samples; const double Seconds=Elapsed-StartedWalking;
            const auto& R=Recipe(C); RegionTimes[int32(R.Region)]=FMath::Min(RegionTimes[int32(R.Region)],Seconds);
            FVector Local(Event->GetNumberField(TEXT("position_x")),Event->GetNumberField(TEXT("position_y")),Event->GetNumberField(TEXT("position_z")));
            // Origin shifts are whole chunks, so the remainder is the local XY.
            Local.X-=FMath::FloorToDouble(Local.X/EW::ChunkSize)*EW::ChunkSize;
            Local.Y-=FMath::FloorToDouble(Local.Y/EW::ChunkSize)*EW::ChunkSize;
            if (Seconds>1800) continue;
            for (const auto& P:R.Places)
            {
                const double Distance=FVector::Dist2D(Local,P.LocalPosition);
                if (Distance>2200 || FoundPlaces.Contains(P.Id)) continue;
                FoundPlaces.Add(P.Id); auto Point=MakeShared<FJsonObject>();
                Point->SetStringField(TEXT("name"),P.Name); Point->SetStringField(TEXT("id"),P.Id);
                Point->SetStringField(TEXT("coord"),C.Text()); Point->SetNumberField(TEXT("seconds"),Seconds);
                Point->SetNumberField(TEXT("horizontal_distance_cm"),Distance);
                Point->SetNumberField(TEXT("capsule_center_above_bookmark_cm"),Local.Z-P.LocalPosition.Z);
                Point->SetStringField(TEXT("place_code"),P.Code()); Opportunities.Add(MakeShared<FJsonValueObject>(Point));
            }
        }
        Check(RouteChecks>0 && Samples>0,TEXT("actual_walking_samples_and_recipe_checks"));
        TArray<TSharedPtr<FJsonValue>> Regions;
        for (int32 I=0;I<3;++I)
        {
            Check(RegionTimes[I]<=1800,TEXT("all_regions_reachable_within_30_minutes"));
            auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("region"),EW::RegionName(EW::RegionKind(I)));
            R->SetNumberField(TEXT("seconds"),RegionTimes[I]==DBL_MAX?-1:RegionTimes[I]); Regions.Add(MakeShared<FJsonValueObject>(R));
        }
        Check(Opportunities.Num()>=2,TEXT("multiple_recording_locations_within_30_minutes"));
        Report->SetBoolField(TEXT("success"),Failures.IsEmpty()); Report->SetArrayField(TEXT("failures"),Failures);
        Report->SetNumberField(TEXT("grounded_samples"),Samples); Report->SetNumberField(TEXT("recipe_digest_checks"),RouteChecks);
        Report->SetArrayField(TEXT("first_region_arrivals"),Regions); Report->SetArrayField(TEXT("recording_opportunities"),Opportunities);
        Report->SetStringField(TEXT("method"),TEXT("Replay real CharacterMovement samples with matching recipe digests. Recording opportunities use the game's 2200 cm horizontal radius in the current loaded chunk."));
        Report->SetStringField(TEXT("not_covered"),TEXT("This does not press E, prove human wayfinding, or replace OS keyboard/mouse playtesting."));
        FString JSON; FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));
        return FFileHelper::SaveStringToFile(JSON,*Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) && Failures.IsEmpty()?0:1;
    }
    FString FixtureDirectory;
    if (FParse::Value(*Params, TEXT("EWSaveFixtureDir="), FixtureDirectory))
    {
        // Only the explicit, synthetic fixture produced by Tools/audit_saves.py is accepted.
        FString Marker;
        if (!FFileHelper::LoadFileToString(Marker, *(FixtureDirectory / TEXT("fixture-marker.txt"))) ||
            Marker != TEXT("endless-world-synthetic-save-fixture-v1")) return 2;
        FEWSaveStore Store;
        Check(Store.Open(FixtureDirectory), TEXT("fixture_open"));
        auto P = EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(), {0,0}).Places[0];
        P.Id = TEXT("fixture-00000");
        Check(Store.Count(P.WorldCode) == EW::DiscoveryQuota, TEXT("actual_10000_records"));
        Check(Store.Record(P), TEXT("duplicate_at_quota_is_idempotent"));
        auto Overflow = P; Overflow.Id = TEXT("fixture-overflow");
        Check(!Store.Record(Overflow) && Store.Error().Contains(TEXT("上限")) && Store.Count(P.WorldCode) == 10000,
            TEXT("record_10001_refused_without_loss"));
        Check(Store.SetFavourite(P.WorldCode, P.Id, true), TEXT("favourite_at_quota"));
        const auto Page = Store.Records(P.WorldCode, 0, 12);
        Check(Page.Num() == 12 && Page[0].Id == P.Id && Page[0].bFavourite, TEXT("quota_pagination_and_sort"));
        Check(Store.SaveCurrent(P), TEXT("position_at_quota"));
        EW::PlaceBookmark Before;
        Check(Store.LoadCurrent(Before) && Before.Id == P.Id && Store.WorldCount() == 1 &&
              Store.Worlds().Num() == 1, TEXT("world_history_at_quota"));
        FString Export;
        Check(Store.Export(P.WorldCode, Export), TEXT("export_10000"));
        FString ExportText; TSharedPtr<FJsonObject> ExportObject;
        const bool Parsed = FFileHelper::LoadFileToString(ExportText, *Export) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ExportText), ExportObject);
        const TArray<TSharedPtr<FJsonValue>>* Exported = nullptr;
        Check(Parsed && ExportObject->TryGetArrayField(TEXT("discoveries"), Exported) && Exported->Num() == 10000,
            TEXT("export_has_all_10000_records"));

        // A directory at the backup destination produces a real filesystem write failure.
        const FString Obstacle = FixtureDirectory / TEXT("exploration.backup-pending.sqlite3");
        const bool MadeObstacle = !IFileManager::Get().FileExists(*Obstacle) &&
            !IFileManager::Get().DirectoryExists(*Obstacle) && IFileManager::Get().MakeDirectory(*Obstacle);
        Check(MadeObstacle, TEXT("backup_failure_fixture_created"));
        auto Changed = P; Changed.LocalPosition.X += 100;
        Check(!Store.SaveCurrent(Changed), TEXT("actual_backup_write_failure_refused"));
        EW::PlaceBookmark After;
        Check(Store.LoadCurrent(After) && After.LocalPosition.Equals(Before.LocalPosition, .001) &&
            Store.Count(P.WorldCode) == 10000 && Store.Integrity(), TEXT("write_failure_preserves_current_and_records"));
        if (MadeObstacle) Check(IFileManager::Get().DeleteDirectory(*Obstacle, false, false), TEXT("remove_empty_test_obstacle"));
        Check(Store.SaveCurrent(P), TEXT("save_retry_after_failure"));
        Store.Close();
        Check(Store.Open(FixtureDirectory) && Store.Count(P.WorldCode) == 10000 && Store.Integrity(), TEXT("fixture_reopen"));
        Store.Close();

        const FString Future = FixtureDirectory / (TEXT("future-version-") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
        IFileManager::Get().MakeDirectory(*Future, true);
        const FString FutureDB = Future / TEXT("exploration.sqlite3");
        Check(IFileManager::Get().Copy(*FutureDB, *(FixtureDirectory / TEXT("exploration.sqlite3")), false) == COPY_OK,
            TEXT("future_version_fixture_copy"));
        { FSQLiteDatabase DB; Check(DB.Open(*FutureDB) && DB.SetUserVersion(99), TEXT("future_version_fixture_mark")); if (DB.IsValid()) DB.Close(); }
        FEWSaveStore FutureStore;
        Check(!FutureStore.Open(Future) && FutureStore.Error().Contains(TEXT("新しい版")), TEXT("future_save_version_rejected"));
        { FSQLiteDatabase DB; int32 V=0; Check(DB.Open(*FutureDB, ESQLiteDatabaseOpenMode::ReadOnly) && DB.GetUserVersion(V) && V==99,
            TEXT("future_version_not_overwritten")); if (DB.IsValid()) DB.Close(); }
        Report->SetBoolField(TEXT("success"), Failures.IsEmpty());
        Report->SetStringField(TEXT("fixture_directory"), FixtureDirectory);
        Report->SetNumberField(TEXT("quota"), EW::DiscoveryQuota);
        Report->SetNumberField(TEXT("exported_records"), Exported ? Exported->Num() : 0);
        Report->SetStringField(TEXT("failure_method"), TEXT("Actual filesystem backup destination obstruction; not physical disk exhaustion"));
        Report->SetArrayField(TEXT("failures"), Failures);
        FString JSON; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&JSON));
        return FFileHelper::SaveStringToFile(JSON, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) && Failures.IsEmpty() ? 0 : 1;
    }
    struct Sample { EW::WorldDescriptor World; EW::ChunkCoord Coord; };
    TArray<Sample> Samples;
    for (int32 Seed = 0; Seed < 100; ++Seed)
    {
        EW::WorldDescriptor W{EW::HashText(FString::Printf(TEXT("endless-core-audit-%d"), Seed)).Left(32)};
        for (int32 I = 0; I < 100; ++I)
        {
            EW::ChunkCoord C;
            if (I == 0) C = {0, 0};
            else if (I == 1) C = {-1, -1};
            else if (I == 2) C = {-100000000000LL, 100000000000LL};
            else if (I == 3) C = {EW::CoordLimit - 1, -EW::CoordLimit + 1};
            else C = {int64(W.Number("audit-x", {I}) % 20001) - 10000, int64(W.Number("audit-y", {I}) % 20001) - 10000};
            Samples.Add({W, C});
        }
    }
    TArray<FString> Digests; Digests.SetNum(Samples.Num());
    int32 Fallbacks = 0, MaxParts = 0, MaxColliders = 0, Edges = 0;
    const double SerialStart = FPlatformTime::Seconds();
    for (int32 I = 0; I < Samples.Num(); ++I)
    {
        auto R = EW::GenerateChunk(Samples[I].World, Samples[I].Coord); FString Error;
        Check(R.Validate(Error), FString::Printf(TEXT("recipe_%d_%s"), I, *Error));
        Digests[I] = R.Digest(); Fallbacks += R.bFallback ? 1 : 0;
        MaxParts = FMath::Max(MaxParts, R.Parts.Num()); MaxColliders = FMath::Max(MaxColliders, R.Colliders.Num());
        for (int32 Axis = 0; Axis < 2; ++Axis)
        {
            auto N = EW::GenerateChunk(Samples[I].World, Samples[I].Coord + (Axis == 0 ? EW::ChunkCoord{1,0} : EW::ChunkCoord{0,1}));
            const auto& A = R.Portals[Axis == 0 ? 1 : 3];
            const auto& B = N.Portals[Axis == 0 ? 0 : 2];
            Check(A.Axis == B.Axis && A.Boundary == B.Boundary && A.Width == B.Width &&
                  A.Position.Z == B.Position.Z &&
                  (Axis == 0 ? A.Position.Y == B.Position.Y : A.Position.X == B.Position.X),
                  FString::Printf(TEXT("shared_edge_%d_%d"), I, Axis));
            ++Edges;
        }
        if (I % 1000 == 0) UE_LOG(LogTemp, Display, TEXT("EW_CORE_PROGRESS %d/%d"), I, Samples.Num());
    }
    Report->SetNumberField(TEXT("serial_seconds_including_edge_checks"), FPlatformTime::Seconds() - SerialStart);
    TArray<FString> Parallel; Parallel.SetNum(Samples.Num());
    ParallelFor(Samples.Num(), [&](int32 I)
    {
        const int32 J = Samples.Num() - I - 1;
        Parallel[J] = EW::GenerateChunk(Samples[J].World, Samples[J].Coord).Digest();
    });
    for (int32 I = 0; I < Samples.Num(); ++I) Check(Digests[I] == Parallel[I], FString::Printf(TEXT("parallel_order_%d"), I));
    Check(Fallbacks == 0, TEXT("normal_worlds_do_not_require_fallback"));
    FString Aggregate;
    for (const auto& Digest : Digests) Aggregate += Digest;
    Report->SetStringField(TEXT("aggregate_digest"), EW::HashText(Aggregate));
    Report->SetNumberField(TEXT("seed_count"), 100); Report->SetNumberField(TEXT("chunks"), Samples.Num());
    Report->SetNumberField(TEXT("shared_edges"), Edges);
    Report->SetNumberField(TEXT("max_parts"), MaxParts); Report->SetNumberField(TEXT("max_colliders"), MaxColliders);
    Report->SetNumberField(TEXT("fallbacks"), Fallbacks);
    EW::WorldDescriptor Parsed; FString Error;
    Check(EW::WorldDescriptor::Parse(EW::WorldDescriptor::ReferenceWorld().Code(), Parsed, Error), TEXT("world_code_round_trip"));
    Check(!EW::WorldDescriptor::Parse(TEXT("ew1-lab1-c286c8776ab548d9b0c1e4a9d2f18fce"), Parsed, Error), TEXT("reject_old_code"));
    Check(!EW::WorldDescriptor::Parse(TEXT("ew2-kit1-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"), Parsed, Error), TEXT("reject_invalid_seed"));
    Check(EW::FloorDiv(-1, EW::ChunkSize) == -1 && EW::FloorDiv(-EW::ChunkSize, EW::ChunkSize) == -1 &&
          EW::FloorDiv(-EW::ChunkSize-1, EW::ChunkSize) == -2, TEXT("negative_coordinate_floor_division"));
    auto R0 = EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(), {0,0});
    Check(R0.Lifts.Num()==4 && R0.CityFloors.Num()>80,TEXT("four_towers_have_upper_and_lower_public_floors"));
    int32 FloorChecks=0;
    for(const auto& L:R0.Lifts)
    {
        Check(L.Stops.Num()>20 && L.Stops[0].Height<R0.Hub.Z-15000 && L.Stops.Last().Height>R0.Hub.Z+15000,
            TEXT("lift_serves_both_vertical_directions_and_roof"));
        for(const auto& S:L.Stops)
        {
            const FVector Saved=S.Landing+FVector(0,0,90);
            const FVector Safe=R0.SafePosition(Saved);
            Check(FMath::Abs(Safe.Z-S.Height-100)<1 && FVector::Dist2D(Safe,Saved)<80,
                FString::Printf(TEXT("height_aware_safe_landing_%s_%.0f"),*L.Id,S.Height));
            EW::PlaceBookmark B=R0.Places[0],ParsedFloor;B.LocalPosition=Saved;
            Check(EW::PlaceBookmark::Parse(B.Code(),ParsedFloor,Error) && FMath::Abs(ParsedFloor.LocalPosition.Z-Saved.Z)<=.5,
                TEXT("upper_lower_place_code_preserves_height"));++FloorChecks;
        }
    }
    Report->SetNumberField(TEXT("upper_lower_floor_save_checks"),FloorChecks);
    EW::PlaceBookmark Place;
    Check(EW::PlaceBookmark::Parse(R0.Places[0].Code(), Place, Error) && Place.Coord == R0.Coord &&
          Place.WorldCode == R0.World.Code() && Place.Id == R0.Places[0].Id, TEXT("place_code_round_trip"));
    Check(!EW::PlaceBookmark::Parse(R0.Places[0].Code() + TEXT("x"), Place, Error), TEXT("place_code_checksum"));
    Check(!EW::PlaceBookmark::Parse(FString::ChrN(300, 'x'), Place, Error), TEXT("place_code_length"));
    auto OutOfRange = EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(), {EW::CoordLimit + 1,0});
    Check(!OutOfRange.Validate(Error), TEXT("coordinate_limit_rejected"));
    Check(EW::RegionAt(R0.World, {0,0}) == EW::RegionKind::City &&
          EW::RegionAt(R0.World, {4,0}) == EW::RegionKind::Garden &&
          EW::RegionAt(R0.World, {0,4}) == EW::RegionKind::Crystal, TEXT("three_arrival_regions"));
    // Regression for a real CharacterMovement stop at garden chunk 3,0:
    // a flat furniture terrace must not cut across a lower ascending path.
    auto RampClearance = [&]()
    {
        const auto R=EW::GenerateChunk(R0.World,{3,0});
        for (const auto& Path : R.Paths) for (int32 I=0;I<=64;++I)
        {
            const FVector P=FMath::Lerp(Path.A,Path.B,I/64.);
            for (const auto& Box : R.Colliders)
            {
                if (!Box.bWalkSurface || !Box.Transform.GetRotation().GetUpVector().Equals(FVector::UpVector,.001)) continue;
                const FVector Q=Box.Transform.InverseTransformPosition(P);
                const double Top=Box.Transform.GetLocation().Z+Box.Extent.Z;
                if (FMath::Abs(Q.X)<Box.Extent.X+38 && FMath::Abs(Q.Y)<Box.Extent.Y+38 &&
                    Top>P.Z+40 && Top-Box.Extent.Z*2<P.Z+176)
                {
                    UE_LOG(LogTemp,Error,TEXT("EW_RAMP_INTRUSION point=%s floor=%s extent=%s rise=%.1f"),
                        *P.ToString(),*Box.Transform.GetLocation().ToString(),*Box.Extent.ToString(),Top-P.Z);
                    return false;
                }
            }
        }
        return true;
    };
    Check(RampClearance(),TEXT("garden_approach_clear_of_raised_terrace_slabs"));

    const FString TestRoot = FPaths::Combine(FPaths::GetPath(Output), TEXT("save-test-") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    FEWSaveStore Store(2);
    Check(Store.Open(TestRoot), TEXT("save_open"));
    auto P0 = R0.Places[0];
    auto P1 = EW::GenerateChunk(R0.World, {1,0}).Places[0];
    auto P2 = EW::GenerateChunk(R0.World, {2,0}).Places[0];
    Check(Store.Record(P0) && Store.Record(P0) && Store.Count(P0.WorldCode) == 1, TEXT("record_idempotent"));
    Check(Store.Record(P1) && Store.Count(P0.WorldCode) == 2, TEXT("record_second"));
    Check(!Store.Record(P2) && Store.Count(P0.WorldCode) == 2, TEXT("quota_preserves_records"));
    Check(Store.SetFavourite(P0.WorldCode, P0.Id, true), TEXT("favourite_write"));
    Check(Store.SaveCurrent(P1), TEXT("position_save"));
    Store.Close();
    Check(Store.Open(TestRoot), TEXT("save_reopen"));
    EW::PlaceBookmark Resumed;
    Check(Store.LoadCurrent(Resumed) && Resumed.Coord == P1.Coord, TEXT("position_resume"));
    auto Book = Store.Records(P0.WorldCode);
    Check(Book.Num() == 2 && Book[0].Id == P0.Id && Book[0].bFavourite, TEXT("favourite_persisted"));
    FString ExportPath;
    Check(Store.Export(P0.WorldCode, ExportPath) && IFileManager::Get().FileExists(*ExportPath), TEXT("discovery_export"));
    const FString Database = Store.Filename();
    Store.Close();
    FEWSaveStore ReadOnly;
    Check(ReadOnly.Open(TestRoot, true), TEXT("read_only_open"));
    Check(!ReadOnly.SaveCurrent(P0) && ReadOnly.Count(P0.WorldCode) == 2, TEXT("write_failure_preserves_save"));
    ReadOnly.Close();
    // Simulate an interrupted SQLite transaction. A separate process crash is tested by the release runner.
    {
        FSQLiteDatabase Transaction;
        Check(Transaction.Open(*Database), TEXT("rollback_open"));
        Check(Transaction.Execute(TEXT("BEGIN IMMEDIATE;")) && Transaction.Execute(TEXT("DELETE FROM discoveries;")), TEXT("rollback_begin"));
        Transaction.Close();
    }
    Check(Store.Open(TestRoot) && Store.Count(P0.WorldCode) == 2, TEXT("uncommitted_transaction_rollback"));
    Store.Close();
    FFileHelper::SaveStringToFile(TEXT("deliberately damaged test database; preserve this file"), *Database);
    Check(Store.Open(TestRoot) && Store.Integrity() && Store.Count(P0.WorldCode) == 2 &&
          !Store.Notice().IsEmpty(), TEXT("corrupt_save_recovery_from_previous"));
    Store.Close();
    Report->SetStringField(TEXT("save_test_directory"), TestRoot);
    Report->SetBoolField(TEXT("success"), Failures.IsEmpty());
    Report->SetArrayField(TEXT("failures"), Failures);
    Report->SetNumberField(TEXT("seconds"), FPlatformTime::Seconds() - Started);
    FString JSON; auto Writer = TJsonWriterFactory<>::Create(&JSON);
    FJsonSerializer::Serialize(Report, Writer);
    const bool Saved = FFileHelper::SaveStringToFile(JSON, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("EW_CORE_AUDIT_%s chunks=%d edges=%d failures=%d output=%s"),
           Failures.IsEmpty() && Saved ? TEXT("PASS") : TEXT("FAIL"), Samples.Num(), Edges, Failures.Num(), *Output);
    return Failures.IsEmpty() && Saved ? 0 : 1;
}
