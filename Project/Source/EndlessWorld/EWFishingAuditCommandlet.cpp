#include "EWFishingAuditCommandlet.h"
#include "EWFishingModel.h"
#include "EWSocialStore.h"
#include "EWSocialProtocol.h"
#include "EWSaveStore.h"
#include "EWWaterCity.h"
#include "EWPhotoMode.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include <limits>

UEWFishingAuditCommandlet::UEWFishingAuditCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UEWFishingAuditCommandlet::Main(const FString& Params)
{
    using namespace EWFishing;
    FString Root;FParse::Value(*Params,TEXT("EWFishingAuditDir="),Root);
    if(Root.IsEmpty() || IFileManager::Get().DirectoryExists(*Root))return 2;
    IFileManager::Get().MakeDirectory(*Root,true);
    int32 Failed=0;TArray<TSharedPtr<FJsonValue>> Results;
    auto Check=[&](FString Name,bool Passed)
    {auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),Name);O->SetBoolField(TEXT("passed"),Passed);Results.Add(MakeShared<FJsonValueObject>(O));if(!Passed){++Failed;UE_LOG(LogTemp,Error,TEXT("FISHING_AUDIT_FAIL %s"),*Name);}};
    Check(TEXT("twelve_species_three_locations"),Species().Num()==12 && Spots().Num()==3);
    TSet<FString> IDs;bool Valid=true;
    for(const auto& S:Species()){Valid&=!IDs.Contains(S.Id) && S.Minimum>0 && S.Maximum>S.Minimum && S.Weight>0;IDs.Add(S.Id);}
    Check(TEXT("unique_fish_catalogue"),Valid);
    Check(TEXT("day_boundaries"),HourBand(4.999)==8 && HourBand(5)==1 && HourBand(9)==2 && HourBand(17)==4 && HourBand(21)==8 && HourBand(23.999)==8);
    Check(TEXT("invalid_hours_rejected"),HourBand(24)==0 && HourBand(-.1)==0 && HourBand(std::numeric_limits<double>::quiet_NaN())==0);
    FRandomStream Random(80);TSet<FString> Encountered;
    for(int32 Site=0;Site<3;++Site)for(int32 Hour=0;Hour<24;++Hour)
    {
        bool OK=true;
        for(int32 Draw=0;Draw<140;++Draw)
        {
            const auto C=Roll(Site,Hour,Random,FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
            OK&=C.Valid();Encountered.Add(C.Species);
        }
        Check(FString::Printf(TEXT("valid_catches_site%d_hour%d"),Site,Hour),OK);
    }
    Check(TEXT("all_species_obtainable"),Encountered.Num()==12);
    Check(TEXT("wrong_site_refused"),!Roll(3,12,Random,TEXT("invalid")).Valid());
    FCast Cast;Check(TEXT("cast_begins_once"),Cast.Begin(0,12,100,true,Random) && !Cast.Begin(0,12,100.1,true,Random));
    Check(TEXT("early_press_does_not_skip_wait"),!Cast.Press(100.2) && Cast.Phase()==EPhase::Waiting);
    Cast.Tick(120);Check(TEXT("bite_waits_without_rapid_input"),Cast.Phase()==EPhase::Bite);
    Cast.Tick(2000);Check(TEXT("patient_mode_has_no_reaction_timeout"),Cast.Phase()==EPhase::Bite);
    Check(TEXT("single_press_reels"),Cast.Press(2000));
    Check(TEXT("repeated_press_does_not_restart_reeling"),!Cast.Press(2001));
    Cast.Tick(2002.81);const auto Receipt=Cast.Result();
    Check(TEXT("landed_result_stable"),Cast.Phase()==EPhase::Landed && Receipt.Valid() && !Cast.Begin(0,12,2010,true,Random));
    Cast.Tick(4000);Check(TEXT("result_not_rerolled_by_frame_ticks"),Cast.Result().Id==Receipt.Id && Cast.Result().Length==Receipt.Length);
    Cast.Cancel();Check(TEXT("cancel_clears_cast"),Cast.Phase()==EPhase::Idle && !Cast.Result().Valid());
    Cast.Begin(1,6,10,false,Random);Cast.Tick(40);Check(TEXT("timed_fish_can_escape"),Cast.Phase()==EPhase::Escaped && !Cast.Press(41));
    Cast.Cancel();Check(TEXT("invalid_start_clock"),!Cast.Begin(0,12,std::numeric_limits<double>::quiet_NaN(),true,Random));

    // Include adjacent procedural districts: an authored water polygon can be
    // below a crossing from a neighbouring chunk even when its own chunk is clear.
    TArray<EW::Collider> CityColliders;
    for(int64 X=-1;X<=1;++X)for(int64 Y=-1;Y<=2;++Y)
    {
        auto District=EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(),{X,Y});
        for(auto C:District.Colliders)
        {C.Transform.AddToTranslation(FVector(X*EW::ChunkSize,Y*EW::ChunkSize,0));CityColliders.Add(C);}
    }
    for(int32 Site=0;Site<3;++Site)
    {
        const auto& S=Spots()[Site];const auto R=EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(),S.Coord);bool Floor=false,Clear=true;
        for(const auto& C:R.Colliders)
        {
            const FVector P=C.Transform.InverseTransformPosition(S.Stand);
            if(C.bWalkSurface && FMath::Abs(P.X)<C.Extent.X-38 && FMath::Abs(P.Y)<C.Extent.Y-38 && FMath::Abs(P.Z-C.Extent.Z-88)<3)Floor=true;
            if(!C.bWalkSurface && FMath::Abs(P.X)<C.Extent.X+36 && FMath::Abs(P.Y)<C.Extent.Y+36 && FMath::Abs(P.Z)<C.Extent.Z+84)Clear=false;
        }
        Check(FString::Printf(TEXT("site%d_has_existing_floor"),Site),Floor);
        if(!Floor)
        {
            FVector Closest=FVector::ZeroVector;double Best=DBL_MAX;
            for(const auto& C:R.Colliders)if(C.bWalkSurface && C.Extent.X>90 && C.Extent.Y>90)
            {
                const FVector L=C.Transform.InverseTransformPosition(S.Stand);
                const FVector Q=C.Transform.TransformPosition({FMath::Clamp(L.X,-C.Extent.X+70,C.Extent.X-70),FMath::Clamp(L.Y,-C.Extent.Y+70,C.Extent.Y-70),C.Extent.Z+88});
                if(FVector::DistSquared(Q,S.Stand)<Best){Best=FVector::DistSquared(Q,S.Stand);Closest=Q;}
            }
            UE_LOG(LogTemp,Display,TEXT("FISHING_NEAREST_FLOOR site=%d position=%s"),Site,*Closest.ToString());
        }
        Check(FString::Printf(TEXT("site%d_capsule_clear_of_walls_rails"),Site),Clear);
        bool Water=false;
        for(const auto& B:EW::GetWaterCityPlan().Bodies)if(FMath::Abs(B.WaterZ+5-S.Float.Z)<1)
        {
            bool Inside=false;
            for(int32 I=0,J=B.Polygon.Num()-1;I<B.Polygon.Num();J=I++)
            {const auto A=B.Polygon[I],Z=B.Polygon[J];if((A.Y>S.Float.Y)!=(Z.Y>S.Float.Y) && S.Float.X<(Z.X-A.X)*(S.Float.Y-A.Y)/(Z.Y-A.Y)+A.X)Inside=!Inside;}
            Water|=Inside;
        }
        Check(FString::Printf(TEXT("site%d_cast_into_real_water"),Site),Water);
        auto Unobstructed=[&](const FVector& WaterPoint)
        {
            for(const auto& C:CityColliders)
            {
                const auto A=C.Transform.InverseTransformPosition(S.Stand+FVector(0,0,74)),B=C.Transform.InverseTransformPosition(WaterPoint);
                if(FMath::LineBoxIntersection(FBox(-C.Extent,C.Extent),A,B,B-A))return false;
            }
            return true;
        };
        Check(FString::Printf(TEXT("site%d_float_visible_from_eye"),Site),Unobstructed(S.Float));
        if(!Unobstructed(S.Float))
        {
            int32 Found=0;
            for(double Y=Site==1?12250:-1000;Y<(Site==1?14200:4100) && Found<12;Y+=100)for(double X=Site==1?5300:5570;X<(Site==1?7500:7270) && Found<12;X+=100)
            {
                const FVector Float(X,Y,S.Float.Z);
                if(FVector::Dist2D(Float,S.Stand)<400 || FVector::Dist2D(Float,S.Stand)>2200 || !Unobstructed(Float))continue;
                UE_LOG(LogTemp,Display,TEXT("FISHING_VISIBLE_FLOAT site=%d float=%s"),Site,*Float.ToString());++Found;
            }
            if(Site==2 && Found==0)
            {
                for(int32 District:{0,-1})
                {
                    const auto Search=EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(),{0,District});
                    const FVector Offset(0,District*EW::ChunkSize,0);
                    for(const auto& FloorBox:Search.Colliders)if(FloorBox.bWalkSurface && FloorBox.Extent.X>95 && FloorBox.Extent.Y>95)
                    for(double XX:{-FloorBox.Extent.X+85,0.,FloorBox.Extent.X-85})for(double YY:{-FloorBox.Extent.Y+85,0.,FloorBox.Extent.Y-85})
                    {
                        if(Found>=8)break;const FVector Stand=FloorBox.Transform.TransformPosition({XX,YY,FloorBox.Extent.Z+88});const auto Global=Stand+Offset;
                        if(Global.X<4800 || Global.X>8000 || Global.Y<-6500 || Global.Y>4500 || Global.Z>2500)continue;
                        bool CandidateClear=true;for(const auto& Box:Search.Colliders)if(!Box.bWalkSurface)
                        {const auto V=Box.Transform.InverseTransformPosition(Stand);if(FMath::Abs(V.X)<Box.Extent.X+36 && FMath::Abs(V.Y)<Box.Extent.Y+36 && FMath::Abs(V.Z)<Box.Extent.Z+84){CandidateClear=false;break;}}
                        if(!CandidateClear)continue;
                        for(double Y=District==0?-1000:-3800;Y<(District==0?5100:-1400) && Found<8;Y+=250)
                        for(double X=5570;X<7270 && Found<8;X+=200)
                        {
                            const FVector Float=FVector(X,Y,District==0?1955:155)-Offset;
                            const double Distance=FVector::Dist2D(Float,Stand);if(Distance<400 || Distance>1600)continue;
                            bool See=true;for(const auto& Box:CityColliders)
                            {const auto A=Box.Transform.InverseTransformPosition(Global+FVector(0,0,74)),B=Box.Transform.InverseTransformPosition(Float+Offset);if(FMath::LineBoxIntersection(FBox(-Box.Extent,Box.Extent),A,B,B-A)){See=false;break;}}
                            if(See){UE_LOG(LogTemp,Display,TEXT("FISHING_SITE_CANDIDATE district=%d stand=%s float=%s"),District,*Stand.ToString(),*Float.ToString());++Found;}
                        }
                    }
                }
            }
        }
    }

    bool DisplaySupported=false;
    for(const auto& Floor:CityColliders)if(Floor.bWalkSurface)
    {
        const FVector Local=Floor.Transform.InverseTransformPosition(DisplayPosition());
        if(FMath::Abs(Local.X)<Floor.Extent.X-30 && FMath::Abs(Local.Y)<Floor.Extent.Y-30 && FMath::Abs(Local.Z-Floor.Extent.Z)<3)DisplaySupported=true;
    }
    Check(TEXT("aquarium_pedestal_has_real_floor"),DisplaySupported);
    const FString Data=Root/TEXT("Data");FEWSaveStore Legacy;Check(TEXT("create_legacy_exploration_fixture"),Legacy.Open(Data));
    const FString LegacyFile=Legacy.Filename();Legacy.Close();TArray<uint8> Before,After;FFileHelper::LoadFileToArray(Before,*LegacyFile);
    // A real phase-1 preference-only database migrates additively without touching identity settings.
    FSQLiteDatabase V1;V1.Open(*(Data/TEXT("social.sqlite3")),ESQLiteDatabaseOpenMode::ReadWriteCreate);
    V1.Execute(TEXT("CREATE TABLE preferences(key TEXT PRIMARY KEY,value TEXT NOT NULL);"));
    V1.Execute(TEXT("INSERT INTO preferences VALUES('nickname','existing');"));V1.Execute(TEXT("PRAGMA user_version=1;"));V1.Close();
    FEWSocialStore Store;Check(TEXT("existing_social_save_opens"),Store.Open(Data) && Store.Get(TEXT("nickname"))==TEXT("existing"));
    bool Added=false;Check(TEXT("durable_fish_catch"),Store.AddCatch(Receipt,Added) && Added);
    Check(TEXT("same_receipt_retry_is_idempotent"),Store.AddCatch(Receipt,Added) && !Added);
    auto Forged=Receipt;Forged.Length+=.1;Check(TEXT("conflicting_receipt_rejected"),!Store.AddCatch(Forged,Added));
    Check(TEXT("unearned_display_refused"),!Store.SetDisplayFish(TEXT("moon-angel")));
    Check(TEXT("earned_display_persists"),Store.SetDisplayFish(Receipt.Species));
    auto Second=Receipt;Second.Id=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);Second.Length=Find(Second.Species)->Maximum;
    Check(TEXT("larger_second_fish_saved"),Store.AddCatch(Second,Added) && Added);
    auto Records=Store.FishRecords();Check(TEXT("maximum_size_and_count_aggregate"),Records.Num()==1 && Records[0].Count==2 && Records[0].Largest==Second.Length);
    Check(TEXT("catch_store_integrity"),Store.Integrity());Store.Backup();Store.Close();
    Check(TEXT("restart_preserves_collection_and_display"),Store.Open(Data) && Store.FishRecords().Num()==1 && Store.FishRecords()[0].Count==2 && Store.Get(TEXT("fish:display"))==Receipt.Species);
    Store.Close();FFileHelper::SaveStringToFile(TEXT("broken fishing save fixture"),*(Data/TEXT("social.sqlite3")));
    Check(TEXT("fish_collection_backup_restores"),Store.Open(Data) && Store.FishRecords().Num()==1 && Store.FishRecords()[0].Count==2 && !Store.Notice().IsEmpty());
    FFileHelper::LoadFileToArray(After,*LegacyFile);Check(TEXT("exploration_save_bytes_preserved"),Before==After);
    Forged=Receipt;Forged.Length=std::numeric_limits<double>::infinity();Check(TEXT("nonfinite_size_refused"),!Store.AddCatch(Forged,Added));
    Forged=Receipt;Forged.Species=TEXT("anything'); DROP TABLE preferences; --");Check(TEXT("unknown_species_refused"),!Store.AddCatch(Forged,Added));
    Check(TEXT("preferences_survive_invalid_input"),Store.Get(TEXT("nickname"))==TEXT("existing") && Store.Integrity());
    for(const FIntPoint Size:{FIntPoint(2560,1440),FIntPoint(1920,1080),FIntPoint(1280,800),FIntPoint(1080,1920),FIntPoint(3440,1440)})for(bool Portrait:{false,true})
    {
        const auto R=EWPhoto::Crop(Size.X,Size.Y,Portrait);
        Check(FString::Printf(TEXT("photo_%dx%d_portrait%d_frame"),Size.X,Size.Y,Portrait),R.Min.X>=0 && R.Min.Y>=0 && R.Max.X<=Size.X && R.Max.Y<=Size.Y &&
            R.Width()*(Portrait?16:9)==R.Height()*(Portrait?9:16) && FMath::Abs(R.Min.X-(Size.X-R.Max.X))<=1 && FMath::Abs(R.Min.Y-(Size.Y-R.Max.Y))<=1);
    }
    Check(TEXT("photo_invalid_dimensions_refused"),EWPhoto::Crop(-1,400,true).Width()==0 && EWPhoto::Crop(MAX_int32,400,false).Width()==0);
    const auto Black=EWPhoto::ToSDR(FLinearColor(-1,0,0,0)),White=EWPhoto::ToSDR(FLinearColor(100,100,100,0));
    Check(TEXT("hdr_png_opaque_black_and_white"),Black==FColor(0,0,0,255) && White.A==255 && White.R>=254);
    const auto BadPixel=EWPhoto::ToSDR(FLinearColor(std::numeric_limits<float>::quiet_NaN(),.5f,1,1));
    Check(TEXT("hdr_nonfinite_pixel_sanitized"),BadPixel.R==0 && BadPixel.G>0 && BadPixel.B>=BadPixel.G);
    auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Failed==0);Report->SetNumberField(TEXT("checks"),Results.Num());
    Report->SetNumberField(TEXT("failures"),Failed);Report->SetArrayField(TEXT("results"),Results);
    Report->SetStringField(TEXT("online_fishing"),TEXT("NOT_IMPLEMENTED - isolated local gameplay until EOS real-service acceptance"));
    FFileHelper::SaveStringToFile(EWSocial::Encode(Report),*(Root/TEXT("audit.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp,Display,TEXT("FISHING_AUDIT checks=%d failures=%d"),Results.Num(),Failed);return Failed?1:0;
}
