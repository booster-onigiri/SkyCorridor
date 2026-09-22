#include "EWSocialAuditCommandlet.h"
#include "EWSocialProtocol.h"
#include "EWWorldClock.h"
#include "EWDayCycle.h"
#include "EWSocialStore.h"
#include "EWEOSConnection.h"
#include "EWSaveStore.h"
#include "EWCinemaPlan.h"
#include "EWBrowserAudioWave.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include <limits>

UEWSocialAuditCommandlet::UEWSocialAuditCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UEWSocialAuditCommandlet::Main(const FString& Params)
{
    using namespace EWSocial;
    FString Directory;FParse::Value(*Params,TEXT("EWSocialAuditDir="),Directory);
    if(Directory.IsEmpty() || IFileManager::Get().DirectoryExists(*Directory))return 2;
    Directory=FPaths::ConvertRelativePathToFull(Directory);IFileManager::Get().MakeDirectory(*Directory,true);
    TArray<TSharedPtr<FJsonValue>> Checks;int32 Failures=0;
    auto Check=[&](const FString& Name,bool Passed)
    {auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),Name);R->SetBoolField(TEXT("passed"),Passed);Checks.Add(MakeShared<FJsonValueObject>(R));if(!Passed){++Failures;UE_LOG(LogTemp,Error,TEXT("SOCIAL_AUDIT_FAIL %s"),*Name);}};
    int64 Coord=0;Check(TEXT("coord_positive_limit"),ReadCoord(LexToString(EW::CoordLimit),Coord) && Coord==EW::CoordLimit);
    Check(TEXT("coord_negative_limit"),ReadCoord(LexToString(-EW::CoordLimit),Coord) && Coord==-EW::CoordLimit);
    for(const FString Bad:{TEXT(""),TEXT("-"),TEXT("1.5"),TEXT("1e8"),TEXT(" 7"),TEXT("+7"),TEXT("1099511627776"),TEXT("9223372036854775807")})
        Check(TEXT("reject_coord_")+Bad,!ReadCoord(Bad,Coord));
    FPose P;P.Chunk={EW::CoordLimit-8,-EW::CoordLimit+8};P.Local={123.125,12799.75,7777.25};P.Yaw=178;P.Sequence=31;
    const auto Json=P.Json();FPose Restored;Check(TEXT("pose_roundtrip_full_precision"),FPose::Read(Json,Restored) && Restored.Chunk==P.Chunk && Restored.Local.Equals(P.Local,.00001));
    const EW::ChunkCoord Origin={P.Chunk.X+8,P.Chunk.Y-8};
    const auto Rebased=FPose::FromRender(Origin,P.RelativeTo(Origin),FRotator(P.Pitch,P.Yaw,0));
    Check(TEXT("render_rebase_preserves_global_position"),Rebased.Chunk==P.Chunk && Rebased.Local.Equals(P.Local,.00001));
    FPose A=P,B=P;B.Local.X+=.125;Check(TEXT("centimetre_precision_at_world_edge"),FMath::IsNearlyEqual(Distance(A,B),.125,.00001));
    auto J=P.Json();J->SetNumberField(TEXT("cx"),1);Check(TEXT("coordinates_must_be_decimal_strings"),!FPose::Read(J,Restored));
    for(double Bad:{-1.,0.,1.5,double(MAX_uint32)+1.,std::numeric_limits<double>::infinity()})
    {J=P.Json();J->SetNumberField(TEXT("sequence"),Bad);Check(TEXT("invalid_sequence_")+LexToString(Bad),!FPose::Read(J,Restored));}
    J=P.Json();J->SetNumberField(TEXT("x"),EW::ChunkSize);Check(TEXT("noncanonical_local_coordinate"),!FPose::Read(J,Restored));
    J=P.Json();J->SetNumberField(TEXT("z"),std::numeric_limits<double>::quiet_NaN());Check(TEXT("nonfinite_pose_rejected"),!FPose::Read(J,Restored));
    J=P.Json();J->SetStringField(TEXT("motion"),TEXT("admin"));Check(TEXT("unknown_motion_rejected"),!FPose::Read(J,Restored));
    TSharedPtr<FJsonObject> Decoded;
    Check(TEXT("malformed_json_rejected"),!Decode(TEXT("{\"pose\":"),Decoded));
    const FString Deep=FString::ChrN(40,TEXT('['))+TEXT("0")+FString::ChrN(40,TEXT(']'));
    Check(TEXT("excessive_nesting_rejected"),!Decode(TEXT("{\"x\":")+Deep+TEXT("}"),Decoded));
    Check(TEXT("quoted_braces_do_not_count_as_depth"),Decode(TEXT("{\"text\":\"[[[{\\\"}]]]\"}"),Decoded));
    Check(TEXT("control_and_bidi_removed"),CleanText(TEXT("  a\n\r\t\u202eb  "),20)==TEXT("ab"));
    Check(TEXT("unicode_name_preserved"),CleanText(TEXT("水鏡の旅人"),20)==TEXT("水鏡の旅人"));
    Check(TEXT("thirty_minute_day"),FMath::IsNearlyEqual(DayHour(7.5,1800),7.5,.000001));
    Check(TEXT("one_game_hour_in_75_seconds"),FMath::IsNearlyEqual(DayHour(10,75),11.,.000001));
    Check(TEXT("midnight_wrap"),FMath::IsNearlyEqual(DayHour(23.9,15),.1,.000001));
    for(double Elapsed:{0.,75.,450.,900.,1350.,1800.,86400.})
        Check(TEXT("local_and_social_clocks_match_")+LexToString(Elapsed),FMath::IsNearlyEqual(DayHour(10,Elapsed),AEWDayCycle::HourAt(Elapsed),.000001));
    Check(TEXT("negative_elapsed_does_not_reverse_clock"),FMath::IsNearlyEqual(DayHour(10,-100),10.,.000001));
    for(double Elapsed:{0.,75.,900.,2099.,3599.,3600.,86400.+2100.})
    {
        const double Now=63900000000.,Epoch=Now-Elapsed,ClockEpoch=EWWorldClock::RebaseEpoch(Now,Epoch,3600.);
        Check(TEXT("legacy_save_keeps_sky_phase_")+LexToString(Elapsed),FMath::IsNearlyEqual(AEWDayCycle::HourAt(Now-ClockEpoch),EWWorldClock::Advance(10,Elapsed,3600.),.000001));
        Check(TEXT("migrated_save_advances_at_new_speed_")+LexToString(Elapsed),FMath::IsNearlyEqual(AEWDayCycle::HourAt(Now-ClockEpoch+75),EWWorldClock::Advance(11,Elapsed,3600.),.000001));
    }
    FHost Host;Host.Reset(TEXT("host"),TEXT("広場の人"),10);
    bool CapacityOK=true;for(int32 I=1;I<=7;++I)CapacityOK&=Host.Admit(LexToString(I),TEXT("旅人"),Protocol,Build,10);
    Check(TEXT("eight_members_including_host"),CapacityOK && Host.Players().Num()==8);
    Check(TEXT("ninth_member_rejected"),!Host.Admit(TEXT("9"),TEXT("旅人"),Protocol,Build,10));
    Check(TEXT("duplicate_admission_idempotent"),Host.Admit(TEXT("1"),TEXT("changed"),Protocol,Build,10) && Host.Players().FindChecked(TEXT("1")).Name==TEXT("旅人"));
    Host.Remove(TEXT("7"));Check(TEXT("incompatible_protocol_rejected"),!Host.Admit(TEXT("7"),TEXT("旅人"),Protocol+1,Build,10));
    Check(TEXT("incompatible_content_rejected"),!Host.Admit(TEXT("7"),TEXT("旅人"),Protocol,TEXT("older"),10));
    Host.Admit(TEXT("7"),TEXT("旅人"),Protocol,Build,10);
    Check(TEXT("unadmitted_sender_cannot_move"),!Host.Pose(TEXT("forged"),P,10));
    Check(TEXT("initial_pose_accepted"),Host.Pose(TEXT("1"),P,10));
    Check(TEXT("duplicate_pose_rejected"),!Host.Pose(TEXT("1"),P,10.1));
    P.Sequence++;P.Local.X+=10;Check(TEXT("normal_walk_accepted"),Host.Pose(TEXT("1"),P,10.1));
    P.Sequence++;P.Chunk.X-=100;Check(TEXT("unapproved_teleport_rejected"),!Host.Pose(TEXT("1"),P,10.2));
    Check(TEXT("single_resource_claim"),Host.Reserve(TEXT("1"),TEXT("cinema/A1")) && !Host.Reserve(TEXT("2"),TEXT("cinema/A1")));
    Check(TEXT("claim_retry_idempotent"),Host.Reserve(TEXT("1"),TEXT("cinema/A1")));
    Host.Remove(TEXT("1"));Check(TEXT("disconnect_releases_claim"),Host.Reserve(TEXT("2"),TEXT("cinema/A1")));
    Host.Ban(TEXT("2"));Check(TEXT("banned_reentry_rejected"),!Host.Admit(TEXT("2"),TEXT("旅人"),Protocol,Build,20));
    Check(TEXT("ban_releases_claim"),Host.Reserve(TEXT("3"),TEXT("cinema/A1")));
    Host.Ban(TEXT("host"));Check(TEXT("host_cannot_ban_self"),Host.Players().Contains(TEXT("host")));
    FString Clean;Check(TEXT("text_rate_limit"),Host.Chat(TEXT("3"),TEXT("こんにちは"),20,Clean) && !Host.Chat(TEXT("3"),TEXT("duplicate"),20.1,Clean));
    Check(TEXT("unadmitted_sender_cannot_chat"),!Host.Chat(TEXT("intruder"),TEXT("hi"),21,Clean));

    const FString Payload=TEXT("{\"text\":\"")+FString::ChrN(8000,TEXT('魚'))+TEXT("\"}");
    auto Packets=Frame(Payload,71,true);FWireInbox Wire;FString Output;int32 Delivered=0;
    for(int32 I=Packets.Num()-1;I>=0;--I)if(Wire.Accept(TEXT("peer"),Packets[I].GetData(),Packets[I].Num(),1,10,Output))++Delivered;
    Check(TEXT("fragmented_unicode_reordered_roundtrip"),Delivered==1 && Output==Payload && Wire.PendingCount()==0);
    bool Duplicate=false;for(const auto& Packet:Packets)Duplicate|=Wire.Accept(TEXT("peer"),Packet.GetData(),Packet.Num(),1,10.5,Output);
    Check(TEXT("replayed_fragments_not_delivered_twice"),!Duplicate);
    auto Corrupt=Packets[0];Corrupt[14]=99;Check(TEXT("wrong_wire_version"),!Wire.Accept(TEXT("peer"),Corrupt.GetData(),Corrupt.Num(),1,11,Output));
    Check(TEXT("wrong_wire_channel"),!Wire.Accept(TEXT("peer"),Packets[0].GetData(),Packets[0].Num(),0,11,Output));
    Check(TEXT("null_packet_rejected"),!Wire.Accept(TEXT("peer"),nullptr,40,1,11,Output));
    Check(TEXT("oversized_utf8_rejected"),Frame(FString::ChrN(30000,TEXT('魚')),99,true).IsEmpty());
    const auto Tiny=Frame(TEXT("{}"),101,true);auto Invalid=Tiny[0];Invalid[HeaderBytes]=0xff;
    Check(TEXT("invalid_utf8_rejected"),!Wire.Accept(TEXT("peer"),Invalid.GetData(),Invalid.Num(),1,12,Output));
    Wire.Reset();
    for(uint32 I=1;I<=12;++I){auto Parts=Frame(Payload,I,true);Wire.Accept(TEXT("flood"),Parts[0].GetData(),Parts[0].Num(),1,20,Output);}
    Check(TEXT("per_peer_incomplete_message_bound"),Wire.PendingCount()==4);
    Check(TEXT("one_peer_cannot_fill_other_peer_quota"),Wire.Accept(TEXT("other"),Tiny[0].GetData(),Tiny[0].Num(),1,20,Output) && Output==TEXT("{}"));
    Wire.Prune(24);Check(TEXT("incomplete_message_expiration"),Wire.PendingCount()==0);
    Wire.Reset();int32 FloodDelivered=0;
    for(uint32 I=1;I<1000;++I){const auto Parts=Frame(TEXT("{}"),I,true);if(Wire.Accept(TEXT("flood"),Parts[0].GetData(),Parts[0].Num(),1,30,Output))++FloodDelivered;}
    Check(TEXT("per_peer_packet_rate_bound"),FloodDelivered<=256);
    Check(TEXT("rate_limit_recovers"),Wire.Accept(TEXT("flood"),Tiny[0].GetData(),Tiny[0].Num(),1,32,Output)==false); // replay is still rejected
    auto Fresh=Frame(TEXT("{}"),2000,true);Check(TEXT("new_packet_after_rate_refill"),Wire.Accept(TEXT("flood"),Fresh[0].GetData(),Fresh[0].Num(),1,32,Output));

    // Real protocol code, deterministic network simulation. This is not a live EOS acceptance test.
    FHost Sim;Sim.Reset(TEXT("host"),TEXT("host"),1);
    for(int32 I=1;I<=7;++I)Sim.Admit(LexToString(I),TEXT("guest"),Protocol,Build,1);
    struct FPending{double At;FString Id;FPose Pose;};TArray<FPending> Delayed;FRandomStream Random(79);int32 Dropped=0,Accepted=0;
    for(int32 Tick=1;Tick<=1200;++Tick)for(int32 Player=1;Player<=7;++Player)
    {
        if(Random.FRand()<.03){++Dropped;continue;}
        FPose Pose;const EW::ChunkCoord Initial={EW::CoordLimit-20,-EW::CoordLimit+20};
        Pose=FPose::FromRender(Initial,FVector(6400+Tick*25.,6400+Player*100.,2338),FRotator::ZeroRotator);Pose.Sequence=Tick;
        Delayed.Add({1+Tick*.05+.150+Random.FRandRange(-.060f,.060f),LexToString(Player),Pose});
    }
    Delayed.Sort([](const FPending& X,const FPending& Y){return X.At<Y.At;});
    for(const auto& Packet:Delayed)if(Sim.Pose(Packet.Id,Packet.Pose,Packet.At))++Accepted;
    bool Latest=true;for(const auto& Pair:Sim.Players())if(Pair.Key!=TEXT("host"))Latest&=Pair.Value.bHasPose && Pair.Value.Pose.Sequence>=1193 && Pair.Value.Pose.Valid();
    Check(TEXT("eight_player_150ms_3pct_loss_model_converges"),Latest && Accepted>4000 && Dropped>0);
    Check(TEXT("cinema_outside_boundary_zero"),EWCinemaPlan::AudioGain(FVector(0,-1101,180))==0 && EWCinemaPlan::AudioGain(FVector(1201,0,180))==0);
    Check(TEXT("cinema_inside_region"),EWCinemaPlan::AudioGain(FVector(0,0,180))>0);
    auto* Wave=NewObject<UEWBrowserAudioWave>();TArray<uint8> PCM;PCM.Init(31,9600);
    for(int32 I=0;I<50;++I)Wave->PushSamples(PCM.GetData(),PCM.Num());
    Check(TEXT("voice_pcm_queue_bounded"),Wave->PendingBytes()<=19200);Wave->FlushSamples();Check(TEXT("voice_pcm_flush_empty"),Wave->PendingBytes()==0);

    const FString SaveDir=Directory/TEXT("Data");FEWSaveStore Legacy;Check(TEXT("legacy_store_fixture_created"),Legacy.Open(SaveDir));
    const FString OldPath=Legacy.Filename();Legacy.Close();TArray<uint8> OldBefore,OldAfter;FFileHelper::LoadFileToArray(OldBefore,*OldPath);
    FEWSocialStore Store;Check(TEXT("social_store_created"),Store.Open(SaveDir));
    Check(TEXT("social_unicode_save"),Store.Put(TEXT("nickname"),TEXT("雲の住人")) && Store.Get(TEXT("nickname"))==TEXT("雲の住人"));
    Check(TEXT("social_bound_sql_quote"),Store.Put(TEXT("literal'key"),TEXT("'; DROP TABLE preferences; --")) && Store.Integrity());
    Check(TEXT("local_block_persistence"),Store.Put(TEXT("block:sample"),TEXT("1")) && Store.Flags(TEXT("block:")).Contains(TEXT("sample")));
    Check(TEXT("social_backup"),Store.Backup());Store.Close();
    Check(TEXT("social_reopen"),Store.Open(SaveDir) && Store.Get(TEXT("nickname"))==TEXT("雲の住人"));Store.Close();
    FFileHelper::SaveStringToFile(TEXT("damaged sqlite fixture"),*(SaveDir/TEXT("social.sqlite3")));
    Check(TEXT("social_corruption_recovers_previous"),Store.Open(SaveDir) && Store.Get(TEXT("nickname"))==TEXT("雲の住人") && !Store.Notice().IsEmpty());Store.Close();
    TArray<FString> Preserved;IFileManager::Get().FindFiles(Preserved,*(SaveDir/TEXT("social.unreadable-*.sqlite3")),true,false);
    Check(TEXT("unreadable_social_file_preserved"),Preserved.Num()==1);
    FSQLiteDatabase Future;Future.Open(*(SaveDir/TEXT("social.sqlite3")),ESQLiteDatabaseOpenMode::ReadWrite);Future.Execute(TEXT("PRAGMA user_version=2;"));Future.Close();
    Check(TEXT("future_social_schema_fails_closed"),!Store.Open(SaveDir));
    FFileHelper::LoadFileToArray(OldAfter,*OldPath);Check(TEXT("legacy_exploration_bytes_unchanged"),OldBefore==OldAfter);
    FEWEOSConnection Offline;Check(TEXT("missing_eos_config_stays_offline"),!Offline.Configure(Directory/TEXT("absent.json"),Directory/TEXT("EOSCache")) && !Offline.LoggedIn() && !Offline.InLobby() && !Offline.VoiceReady());
    Offline.Authenticate();Check(TEXT("commandlet_does_not_fake_anonymous_login"),!Offline.LoggedIn());
    const FString ConfigPath=Directory/TEXT("synthetic-config.json");auto Config=MakeShared<FJsonObject>();
    for(const TCHAR* Key:{TEXT("ProductId"),TEXT("SandboxId"),TEXT("DeploymentId"),TEXT("ClientId"),TEXT("ClientSecret")})Config->SetStringField(Key,TEXT("synthetic-format-only"));
    FFileHelper::SaveStringToFile(Encode(Config),*ConfigPath);
    Check(TEXT("synthetic_config_format_validation_only"),Offline.Configure(ConfigPath,Directory/TEXT("EOSCache")) && !Offline.LoggedIn());
    Offline.Authenticate();Check(TEXT("synthetic_credentials_never_used_by_commandlet"),!Offline.LoggedIn());
    Config->SetNumberField(TEXT("ClientId"),123);FFileHelper::SaveStringToFile(Encode(Config),*ConfigPath);
    Check(TEXT("config_rejects_numeric_credentials_and_clears_ready"),!Offline.Configure(ConfigPath,Directory/TEXT("EOSCache")) && !Offline.Configured());
    Config->SetStringField(TEXT("ClientId"),TEXT(" \t "));FFileHelper::SaveStringToFile(Encode(Config),*ConfigPath);
    Check(TEXT("config_rejects_blank_credentials"),!Offline.Configure(ConfigPath,Directory/TEXT("EOSCache")));
    auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Failures==0);Report->SetNumberField(TEXT("checks"),Checks.Num());Report->SetNumberField(TEXT("failures"),Failures);
    Report->SetArrayField(TEXT("results"),Checks);Report->SetNumberField(TEXT("simulated_pose_messages_accepted"),Accepted);Report->SetNumberField(TEXT("simulated_packets_dropped"),Dropped);
    Report->SetStringField(TEXT("live_eos_8_people"),TEXT("NOT_TESTED - product configuration and distinct devices required"));
    Report->SetStringField(TEXT("physical_voice_and_room_leakage"),TEXT("NOT_MEASURED - real voice service and endpoints required"));
    Report->SetStringField(TEXT("one_hour_soak"),TEXT("NOT_TESTED"));
    FFileHelper::SaveStringToFile(Encode(Report),*(Directory/TEXT("audit.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp,Display,TEXT("SOCIAL_AUDIT checks=%d failures=%d"),Checks.Num(),Failures);return Failures==0?0:1;
}
