#include "EWResidenceAuditCommandlet.h"
#include "EWWorld.h"
#include "EWCityAccess.h"
#include "EWSaveStore.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> VectorJSON(FVector V)
{ return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }

bool SameCollider(const EW::Collider& A,const EW::Collider& B)
{ return A.Transform.Equals(B.Transform,.01) && A.Extent.Equals(B.Extent,.01) && A.bWalkSurface==B.bWalkSurface; }

// Conservative axis-aligned bounds of the real 38/88 cm capsule in each
// yaw-only room collider. Actual movement and view/sound acceptance are separate.
bool CapsuleClear(const EW::ChunkRecipe& R,FVector Centre,double HalfHeight=88.)
{
    for(const auto& C:R.Colliders)
    {
        const FVector Q=C.Transform.InverseTransformPosition(Centre);
        const FVector U=C.Transform.GetRotation().UnrotateVector(FVector::UpVector).GetAbs();
        const FVector E=FVector(38)+U*(HalfHeight-38.);
        if(FMath::Abs(Q.X)<C.Extent.X+E.X-.1 && FMath::Abs(Q.Y)<C.Extent.Y+E.Y-.1 &&
           FMath::Abs(Q.Z)<C.Extent.Z+E.Z-.1)return false;
    }
    return true;
}
bool SegmentHits(const EW::Collider& C,FVector A,FVector B)
{
    A=C.Transform.InverseTransformPosition(A);B=C.Transform.InverseTransformPosition(B);
    const FVector D=B-A;double First=0.,Last=1.;
    for(int32 Axis=0;Axis<3;++Axis)
    {
        const double E=C.Extent[Axis];
        if(FMath::Abs(D[Axis])<.0001){if(A[Axis]<-E || A[Axis]>E)return false;continue;}
        const double X=(-E-A[Axis])/D[Axis],Y=(E-A[Axis])/D[Axis];
        First=FMath::Max(First,FMath::Min(X,Y));Last=FMath::Min(Last,FMath::Max(X,Y));if(First>Last)return false;
    }
    return First<1. && Last>0.;
}
bool Supported(const EW::ChunkRecipe& R,FVector Centre)
{
    for(const auto& C:R.Colliders)if(C.bWalkSurface)
    {
        const FVector Q=C.Transform.InverseTransformPosition(Centre-FVector(0,0,88));
        if(FMath::Abs(Q.X)<=C.Extent.X+.1 && FMath::Abs(Q.Y)<=C.Extent.Y+.1 &&
           FMath::Abs(Q.Z-C.Extent.Z)<.5)return true;
    }
    return false;
}
bool OptionalTableExists(const FString& Filename,bool& Exists)
{
    FSQLiteDatabase DB;
    if(!DB.Open(*Filename,ESQLiteDatabaseOpenMode::ReadOnly))return false;
    bool OK=false;Exists=false;
    {
        auto S=DB.PrepareStatement(TEXT("SELECT 1 FROM sqlite_master WHERE type='table' AND name='residence_state';"));
        if(S.IsValid())
        {
            const auto Step=S.Step();Exists=Step==ESQLitePreparedStatementStepResult::Row;
            OK=Exists || Step==ESQLitePreparedStatementStepResult::Done;
        }
    }
    DB.Close();return OK;
}
bool LegacyView(FEWSaveStore& Store,const EW::PlaceBookmark& Expected)
{
    EW::PlaceBookmark Current,WorldPosition;const auto Records=Store.Records(Expected.WorldCode,0,12);
    return Store.Count(Expected.WorldCode)==1 && Store.WorldCount()==1 && Records.Num()==1 &&
        Records[0].Id==Expected.Id && Records[0].bFavourite && Store.LoadCurrent(Current) &&
        Current.WorldCode==Expected.WorldCode && Current.Id==Expected.Id &&
        Current.LocalPosition.Equals(Expected.LocalPosition,.001) && FMath::Abs(Current.Yaw-Expected.Yaw)<.001 &&
        Store.LoadWorldPosition(Expected.WorldCode,WorldPosition) && WorldPosition.Id==Expected.Id &&
        WorldPosition.LocalPosition.Equals(Expected.LocalPosition,.001);
}
}

UEWResidenceAuditCommandlet::UEWResidenceAuditCommandlet()
{ IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;ShowErrorCount=true; }

int32 UEWResidenceAuditCommandlet::Main(const FString& Params)
{
    const double Started=FPlatformTime::Seconds();
    FString Output;
    if(!FParse::Value(*Params,TEXT("EWReport="),Output) || IFileManager::Get().FileExists(*Output))
    { UE_LOG(LogTemp,Error,TEXT("EW_RESIDENCE_AUDIT requires a new -EWReport=<path>"));return 2; }
    Output=FPaths::ConvertRelativePathToFull(Output);
    if(!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output),true))return 2;
    auto Report=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Checks,Failures;
    auto Check=[&](bool OK,const FString& Name)
    {
        auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("name"),Name);Item->SetBoolField(TEXT("pass"),OK);
        Checks.Add(MakeShared<FJsonValueObject>(Item));
        if(!OK){Failures.Add(MakeShared<FJsonValueString>(Name));UE_LOG(LogTemp,Error,TEXT("EW_RESIDENCE_AUDIT_FAIL %s"),*Name);}
    };
    auto Finish=[&]()
    {
        Report->SetBoolField(TEXT("success"),Failures.IsEmpty());Report->SetArrayField(TEXT("checks"),Checks);
        Report->SetArrayField(TEXT("failures"),Failures);Report->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
        Report->SetStringField(TEXT("not_covered"),TEXT("Recipe geometry and isolated SQLite checks do not prove CharacterMovement, visual composition, audible differences, input hardware, or performance."));
        FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));
        return FFileHelper::SaveStringToFile(JSON,*Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) && Failures.IsEmpty()?0:1;
    };
    const EW::WorldDescriptor W=EW::WorldDescriptor::ReferenceWorld();
    const auto R=EW::GenerateChunk(W,{0,0});
    EW::PlaceBookmark Bookmark;Bookmark.WorldCode=W.Code();Bookmark.Coord={0,0};Bookmark.Id=TEXT("residence-audit-bookmark");
    Bookmark.Name=TEXT("窓辺検証用の既存記録");Bookmark.Kind=0;Bookmark.Yaw=33.;
    Bookmark.LocalPosition=R.Residences.Num()==1?R.SafePosition(R.Residences[0].Stand):R.Hub+FVector(0,-1250,100);
    const FString Id=TEXT("rain-window/0,0/nw");

    FString Replay;
    if(FParse::Value(*Params,TEXT("EWResidenceReplay="),Replay))
    {
        FString JSON,Root,Token,Marker;TSharedPtr<FJsonObject> Source;
        if(!FFileHelper::LoadFileToString(JSON,*Replay) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JSON),Source) ||
           !Source.IsValid() || !Source->GetBoolField(TEXT("success")) ||
           !Source->TryGetStringField(TEXT("fixture_directory"),Root) || !Source->TryGetStringField(TEXT("fixture_token"),Token))return 2;
        Root=FPaths::ConvertRelativePathToFull(Root);FPaths::NormalizeDirectoryName(Root);
        FString Allowed=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("ResidenceAudit"));FPaths::NormalizeDirectoryName(Allowed);
        if(!Root.StartsWith(Allowed+TEXT("/"),ESearchCase::IgnoreCase) || Token.Len()!=32 ||
           !FFileHelper::LoadFileToString(Marker,*(Root/TEXT("synthetic-residence-fixture.txt"))) ||
           Marker!=TEXT("rain-window-isolated-fixture-v1 ")+Token)return 2;
        Report->SetStringField(TEXT("mode"),TEXT("separate-process-read-only-replay"));Report->SetStringField(TEXT("fixture_directory"),Root);
        TArray<uint8> Before,After;const FString Filename=Root/TEXT("exploration.sqlite3");
        Check(FFileHelper::LoadFileToArray(Before,*Filename),TEXT("replay_snapshot"));
        FEWSaveStore Store;int32 Mode=-1;
        const bool Open=Store.Open(Root,true);Check(Open,TEXT("replay_read_only_open"));
        Check(Open && Store.LoadResidenceState(W.Code(),Id,1,Mode) && Mode==1,TEXT("mode_one_survives_separate_process"));
        Check(Open && LegacyView(Store,Bookmark),TEXT("legacy_bookmark_and_favourite_survive_separate_process"));Store.Close();
        Check(FFileHelper::LoadFileToArray(After,*Filename) && Before==After,TEXT("replay_leaves_database_bytes_unchanged"));
        return Finish();
    }

    Report->SetStringField(TEXT("mode"),TEXT("recipe-and-isolated-save-audit"));Report->SetStringField(TEXT("world"),W.Code());
    Report->SetStringField(TEXT("recipe_digest"),R.Digest());
    FString Error;Check(R.Validate(Error) && !R.bFallback,TEXT("reference_recipe_valid"));
    Check(R.Residences.Num()==1,TEXT("one_room_in_reference_district"));
    Check(R.Lifts.Num()==4,TEXT("four_existing_lifts"));
    Check(EW::GeneratorVersion==2 && EW::CatalogueVersion==1 && W.Code()==TEXT("ew2-kit1-c286c8776ab548d9b0c1e4a9d2f18fce"),TEXT("world_code_and_random_domain_versions_preserved"));
    Check(R.Digest()==EW::GenerateChunk(W,{0,0}).Digest(),TEXT("deterministic_reference_recipe"));
    bool Isolated=true,AllValid=true;
    for(const EW::ChunkCoord C:{EW::ChunkCoord{-1,0},EW::ChunkCoord{1,0},EW::ChunkCoord{0,-1},EW::ChunkCoord{0,1},EW::ChunkCoord{5,1},EW::ChunkCoord{1,5}})
    {
        const auto Other=EW::GenerateChunk(W,C);Isolated&=Other.Residences.IsEmpty();AllValid&=Other.Validate(Error) && !Other.bFallback;
    }
    for(int32 I=0;I<16;++I)
    {
        const EW::WorldDescriptor Other{EW::HashText(FString::Printf(TEXT("residence-isolation-%d"),I)).Left(32)};
        const auto Recipe=EW::GenerateChunk(Other,{0,0});Isolated&=Recipe.Residences.IsEmpty();AllValid&=Recipe.Validate(Error) && !Recipe.bFallback;
    }
    Check(Isolated,TEXT("no_residence_added_to_other_chunks_or_worlds"));Check(AllValid,TEXT("neighbor_and_other_world_recipes_valid"));
    Check(R.Places.Num()<=1 && !R.Places.ContainsByPredicate([](const EW::PlaceBookmark& P){return P.Kind<0 || P.Kind>8;}),TEXT("existing_discovery_contract"));
    if(R.Residences.Num()!=1)return Finish();
    const auto& S=R.Residences[0];
    auto Layout=MakeShared<FJsonObject>();Layout->SetStringField(TEXT("format"),TEXT("rain-window-layout-v1"));
    Layout->SetNumberField(TEXT("feature_revision"),S.Revision);Layout->SetStringField(TEXT("residence_id"),S.Id);
    Layout->SetNumberField(TEXT("block_variant"),FCString::Atoi(*S.OriginalMesh.ToString().Right(1)));
    Layout->SetStringField(TEXT("original_mesh"),S.OriginalMesh.ToString());Layout->SetNumberField(TEXT("opening_side"),S.OpeningSide);
    Layout->SetNumberField(TEXT("opening_floor_m"),S.FloorOffset/100.);Layout->SetNumberField(TEXT("public_floor_z_cm"),S.Frame.GetLocation().Z);
    Layout->SetArrayField(TEXT("frame_position_chunk_cm"),VectorJSON(S.Frame.GetLocation()));
    Layout->SetArrayField(TEXT("frame_rotation_euler"),VectorJSON(S.Frame.Rotator().Euler()));Layout->SetArrayField(TEXT("frame_scale"),VectorJSON(S.Frame.GetScale3D()));
    Layout->SetArrayField(TEXT("block_position_chunk_cm"),VectorJSON(S.BlockTransform.GetLocation()));
    Layout->SetArrayField(TEXT("block_rotation_euler"),VectorJSON(S.BlockTransform.Rotator().Euler()));Layout->SetArrayField(TEXT("block_scale"),VectorJSON(S.BlockTransform.GetScale3D()));
    Layout->SetArrayField(TEXT("entry_chunk_cm"),VectorJSON(S.Entry));Layout->SetArrayField(TEXT("valve_chunk_cm"),VectorJSON(S.Valve));
    Layout->SetArrayField(TEXT("seat_chunk_cm"),VectorJSON(S.Seat));Layout->SetArrayField(TEXT("stand_chunk_cm"),VectorJSON(S.Stand));
    Layout->SetNumberField(TEXT("view_yaw"),S.ViewYaw);Layout->SetNumberField(TEXT("view_pitch"),S.ViewPitch);
    Layout->SetArrayField(TEXT("view_target_chunk_cm"),VectorJSON(S.ViewTarget));Layout->SetStringField(TEXT("view_target_mesh"),TEXT("ClockTower west clock hand"));
    Report->SetObjectField(TEXT("layout"),Layout);

    // Rebuild access once with the original closed block. This compares both
    // branches using the same final building transforms, including all stops.
    EW::ChunkRecipe Access;Access.World=W;Access.Coord={0,0};Access.Hub=R.Hub;Access.Region=EW::RegionKind::City;Access.Portals=R.Portals;
    EW::BuildCityGround(Access);
    for(const auto& P:R.Parts)if(P.Mesh==TEXT("ClockTower"))Access.Parts.Add(P);
    EW::CityVolumeParts(W,{0,0},Access.Parts);auto Closed=Access;int32 Replaced=0;
    for(auto& P:Closed.Parts)if(P.Mesh==TEXT("UrbanBlock_RainWindow")){P.Mesh=S.OriginalMesh;++Replaced;}
    EW::BuildCityAccess(Access);EW::BuildCityAccess(Closed);
    auto Normalized=Access;for(auto& P:Normalized.Parts)if(P.Mesh==TEXT("UrbanBlock_RainWindow"))P.Mesh=S.OriginalMesh;
    Normalized.Colliders.Reset();Normalized.Residences.Reset();auto Baseline=Closed;Baseline.Colliders.Reset();Baseline.Residences.Reset();
    Check(Replaced==1 && Normalized.Digest()==Baseline.Digest(),TEXT("only_one_mesh_changes_and_all_paths_floors_lifts_remain_identical"));
    int32 MissingOld=0;
    for(const auto& C:Closed.Colliders)if(!Access.Colliders.ContainsByPredicate([&](const EW::Collider& N){return SameCollider(C,N);}))++MissingOld;
    Check(MissingOld==1,TEXT("only_original_room_block_collider_is_replaced"));
    Check(Access.Lifts.Num()==4 && Access.Lifts.ContainsByPredicate([&](const EW::LiftSpec& L)
        {return L.Stops.ContainsByPredicate([&](const EW::LiftStop& F){return FMath::Abs(F.Height-S.Frame.GetLocation().Z)<.1;});}),TEXT("residence_floor_has_existing_public_lift_stop"));

    TArray<FVector> Route{S.Entry,S.Frame.TransformPosition(FVector(225,-1450,88)),S.Frame.TransformPosition(FVector(450,-1450,88)),
        S.Frame.TransformPosition(FVector(450,-1600,88)),S.Frame.TransformPosition(FVector(675,-1600,88))};
    TArray<TSharedPtr<FJsonValue>> RouteJSON;for(const auto P:Route)RouteJSON.Add(MakeShared<FJsonValueArray>(VectorJSON(P)));
    Layout->SetArrayField(TEXT("room_route_chunk_cm"),RouteJSON);
    bool Clear=true,Ground=true;int32 Samples=0;TArray<TSharedPtr<FJsonValue>> BadPoints;
    for(int32 I=1;I<Route.Num();++I)
    {
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist(Route[I-1],Route[I])/20.));
        for(int32 J=0;J<=Steps;++J)
        {
            const FVector P=FMath::Lerp(Route[I-1],Route[I],double(J)/Steps);const bool C=CapsuleClear(R,P),G=Supported(R,P);
            Clear&=C;Ground&=G;++Samples;if((!C || !G) && BadPoints.Num()<10)BadPoints.Add(MakeShared<FJsonValueArray>(VectorJSON(P)));
        }
    }
    Check(Clear,TEXT("entry_to_window_route_clears_capsule_bounds"));Check(Ground,TEXT("entry_to_window_route_has_continuous_floor"));
    Report->SetNumberField(TEXT("route_samples"),Samples);Report->SetArrayField(TEXT("bad_route_points"),BadPoints);
    Check(CapsuleClear(R,S.Stand) && Supported(R,S.Stand),TEXT("stand_return_is_clear_and_supported"));
    const FVector SafeStand=R.SafePosition(S.Stand);
    Check(CapsuleClear(R,S.Seat,42.),TEXT("seated_capsule_clears_bench_window_and_guard"));
    Check(FVector::Dist2D(SafeStand,S.Stand)<.1 && FMath::Abs(SafeStand.Z-(S.Frame.GetLocation().Z+100))<.1,TEXT("safe_position_retains_standing_bookmark"));
    Check(!CapsuleClear(R,S.Frame.TransformPosition(FVector(-225,-1450,88))) &&
          !CapsuleClear(R,S.Frame.TransformPosition(FVector(1125,-1450,88))) &&
          !CapsuleClear(R,S.Frame.TransformPosition(FVector(450,-1000,88))),TEXT("adjacent_rooms_and_back_wall_stay_closed"));
    Check(!CapsuleClear(R,S.Frame.TransformPosition(FVector(675,-1780,88))),TEXT("window_sill_and_guard_block_walking_out"));

    const FVector Eye=S.Seat+FVector(0,0,47),LocalEye=S.Frame.InverseTransformPosition(Eye),LocalTarget=S.Frame.InverseTransformPosition(S.ViewTarget);
    const double WindowT=(-1822.-LocalEye.Y)/(LocalTarget.Y-LocalEye.Y);
    const FVector AtWindow=FMath::Lerp(LocalEye,LocalTarget,WindowT);
    Check(WindowT>0 && WindowT<1 && AtWindow.X>550 && AtWindow.X<800 && AtWindow.Z>111 && AtWindow.Z<315,TEXT("clock_view_passes_inside_open_window_above_guard"));
    Layout->SetArrayField(TEXT("clock_ray_at_outer_window_frame_cm"),VectorJSON(AtWindow));
    TArray<TSharedPtr<FJsonValue>> Blocked,ArrivalBlocked,BlockedDetails;
    const FVector ArrivalEye(6400,5150,R.Hub.Z+162);
    for(int32 I=0;I<R.Colliders.Num();++I)
    {
        const auto& C=R.Colliders[I];
        // The clock's conservative solid body covers its actual clock hands.
        // A ray aimed at that mesh is allowed to terminate on its own body.
        const bool ClockBody=C.Extent.Equals(FVector(160,160,380),.01) && C.Transform.GetLocation().Equals(R.Hub+FVector(0,0,380),.01);
        if(!ClockBody && SegmentHits(C,Eye,S.ViewTarget))
        {
            Blocked.Add(MakeShared<FJsonValueNumber>(I));auto Detail=MakeShared<FJsonObject>();Detail->SetNumberField(TEXT("index"),I);
            Detail->SetArrayField(TEXT("centre"),VectorJSON(C.Transform.GetLocation()));Detail->SetArrayField(TEXT("extent"),VectorJSON(C.Extent));
            Detail->SetBoolField(TEXT("floor"),C.bWalkSurface);BlockedDetails.Add(MakeShared<FJsonValueObject>(Detail));
        }
        if(SegmentHits(C,Eye,ArrivalEye))ArrivalBlocked.Add(MakeShared<FJsonValueNumber>(I));
    }
    Check(Blocked.IsEmpty(),TEXT("actual_arrival_clock_sightline_clears_recipe_colliders"));
    Report->SetArrayField(TEXT("clock_sightline_blocking_colliders"),Blocked);
    Report->SetArrayField(TEXT("clock_sightline_blocking_details"),BlockedDetails);
    Report->SetArrayField(TEXT("arrival_eye_sightline_blocking_colliders"),ArrivalBlocked);
    Report->SetBoolField(TEXT("arrival_eye_sightline_clear"),ArrivalBlocked.IsEmpty());
    Report->SetStringField(TEXT("sightline_method"),TEXT("Seat eye to an actual ClockTower hand point, using the generated landmark transform. Only that target's own conservative collider is excluded. Mesh-level arcade and composition acceptance remain rendered checks."));

    const FString Token=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("ResidenceAudit")/Token);
    if(IFileManager::Get().DirectoryExists(*Root) || !IFileManager::Get().MakeDirectory(*Root,true))return 2;
    Report->SetStringField(TEXT("fixture_directory"),Root);Report->SetStringField(TEXT("fixture_token"),Token);
    if(!FFileHelper::SaveStringToFile(TEXT("rain-window-isolated-fixture-v1 ")+Token,*(Root/TEXT("synthetic-residence-fixture.txt")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return 2;
    FEWSaveStore Store;const bool Open=Store.Open(Root);Check(Open,TEXT("new_isolated_fixture_open"));if(!Open)return Finish();
    const FString Filename=Store.Filename();int32 Mode=-1;bool Table=false;
    const bool DefaultMode=Store.LoadResidenceState(W.Code(),Id,1,Mode) && Mode==0;Store.Close();
    Check(DefaultMode && OptionalTableExists(Filename,Table) && !Table,TEXT("legacy_save_defaults_to_eaves_without_creating_optional_table"));
    Check(Store.Open(Root),TEXT("reopen_after_optional_table_probe"));
    Check(Store.Record(Bookmark) && Store.SetFavourite(W.Code(),Bookmark.Id,true) && Store.SaveCurrent(Bookmark) && LegacyView(Store,Bookmark),TEXT("legacy_fixture_record_favourite_and_position"));
    Check(Store.SaveResidenceState(W.Code(),Id,1,1) && Store.LoadResidenceState(W.Code(),Id,1,Mode) && Mode==1,TEXT("window_flow_commit_and_read"));
    Check(LegacyView(Store,Bookmark),TEXT("optional_table_keeps_legacy_data"));
    const EW::WorldDescriptor Other{EW::HashText(TEXT("residence-other-world-save")).Left(32)};
    Check(Store.LoadResidenceState(Other.Code(),Id,1,Mode) && Mode==0,TEXT("residence_choice_is_per_world"));
    Check(!Store.SaveResidenceState(W.Code(),Id,1,2) && Store.LoadResidenceState(W.Code(),Id,1,Mode) && Mode==1,TEXT("invalid_mode_rejected_without_changing_choice"));
    Store.Close();
    TArray<uint8> Before,After;Check(FFileHelper::LoadFileToArray(Before,*Filename),TEXT("read_only_snapshot"));
    Check(Store.Open(Root,true) && Store.LoadResidenceState(W.Code(),Id,1,Mode) && Mode==1,TEXT("window_flow_survives_connection_reopen"));
    Check(!Store.SaveResidenceState(W.Code(),Id,1,0) && Store.LoadResidenceState(W.Code(),Id,1,Mode) && Mode==1,TEXT("read_only_write_failure_retains_previous_flow"));Store.Close();
    Check(FFileHelper::LoadFileToArray(After,*Filename) && Before==After,TEXT("read_only_failure_leaves_database_bytes_unchanged"));
    Check(Store.Open(Root) && Store.SaveResidenceState(W.Code(),Id,1,0) && Store.LoadResidenceState(W.Code(),Id,1,Mode) && Mode==0,TEXT("eaves_flow_can_be_restored"));
    Check(LegacyView(Store,Bookmark) && Store.Integrity(),TEXT("legacy_data_and_integrity_survive_both_flow_states"));
    Check(Store.SaveResidenceState(W.Code(),Id,1,1),TEXT("prepare_window_flow_for_separate_process_replay"));Store.Close();
    {
        FSQLiteDatabase DB;int32 Version=-1;
        Check(DB.Open(*Filename,ESQLiteDatabaseOpenMode::ReadOnly) && DB.GetUserVersion(Version) && Version==1 && DB.PerformQuickIntegrityCheck(),TEXT("sqlite_user_version_remains_one"));
        if(DB.IsValid())DB.Close();
    }
    const FString FutureRoot=Root/TEXT("future-feature"),FutureFile=FutureRoot/TEXT("exploration.sqlite3");
    const bool Copy=IFileManager::Get().MakeDirectory(*FutureRoot,true) && IFileManager::Get().Copy(*FutureFile,*Filename,false,false)==COPY_OK;
    Check(Copy,TEXT("isolated_future_feature_fixture_created"));
    if(Copy)
    {
        bool Modified=false;{ FSQLiteDatabase DB;if(DB.Open(*FutureFile,ESQLiteDatabaseOpenMode::ReadWrite))
            { Modified=DB.Execute(TEXT("UPDATE residence_state SET revision=2;"));DB.Close(); } }
        Check(Modified && Store.Open(FutureRoot),TEXT("future_feature_fixture_open"));
        Check(!Store.LoadResidenceState(W.Code(),Id,1,Mode) && !Store.SaveResidenceState(W.Code(),Id,1,0),TEXT("future_feature_revision_is_not_overwritten"));Store.Close();
        // The Windows SQLite platform file has an exclusive read handle. Take
        // byte snapshots only while it is closed. A read-only reopen does not
        // include Open(RW)'s existing schema transaction in this comparison.
        Before.Reset();After.Reset();const bool Snapshot=FFileHelper::LoadFileToArray(Before,*FutureFile);
        Check(Store.Open(FutureRoot,true) && !Store.LoadResidenceState(W.Code(),Id,1,Mode) &&
              !Store.SaveResidenceState(W.Code(),Id,1,0),TEXT("future_feature_read_only_reopen_also_rejects_revision"));Store.Close();
        Check(Snapshot && FFileHelper::LoadFileToArray(After,*FutureFile) && Before.Num()>0 && Before==After,TEXT("future_feature_failure_preserves_database_bytes"));
        bool FutureChoice=false;
        { FSQLiteDatabase DB;if(DB.Open(*FutureFile,ESQLiteDatabaseOpenMode::ReadOnly))
            {
                { auto Query=DB.PrepareStatement(TEXT("SELECT revision,flow_mode FROM residence_state;"));int32 Revision=-1,Choice=-1;
                  FutureChoice=Query.IsValid() && Query.Step()==ESQLitePreparedStatementStepResult::Row &&
                    Query.GetColumnValueByIndex(0,Revision) && Query.GetColumnValueByIndex(1,Choice) && Revision==2 && Choice==1; }
                DB.Close();
            } }
        Check(FutureChoice,TEXT("future_revision_and_choice_remain_intact_after_read_write_attempt"));
    }
    return Finish();
}
