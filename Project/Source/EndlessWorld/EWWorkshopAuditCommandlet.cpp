#include "EWWorkshopAuditCommandlet.h"
#include "EWWorkshopData.h"
#include "EWChessRules.h"
#include "EWSkyTheatrePlan.h"
#include "EWCinemaPlan.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
uint64 Perft(const EWChess::Position& P,int Depth)
{if(!Depth)return 1;uint64 Count=0;for(const auto& M:P.Legal())Count+=Perft(P.After(M),Depth-1);return Count;}
bool Has(const EWChess::Position& P,const char* Uci)
{for(const auto& M:P.Legal())if(M.Uci()==Uci)return true;return false;}
}
UEWWorkshopAuditCommandlet::UEWWorkshopAuditCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UEWWorkshopAuditCommandlet::Main(const FString& Params)
{
    FString Report;FParse::Value(*Params,TEXT("EWReport="),Report);if(Report.IsEmpty() || IFileManager::Get().FileExists(*Report))return 2;
    TArray<TSharedPtr<FJsonValue>> Checks;int32 Failed=0;
    auto Check=[&](const TCHAR* Name,bool OK){auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("check"),Name);C->SetBoolField(TEXT("pass"),OK);Checks.Add(MakeShared<FJsonValueObject>(C));if(!OK){++Failed;UE_LOG(LogTemp,Error,TEXT("WORKSHOP_FAIL %s"),Name);}};
    using namespace EWChess;Position P=Position::Initial();
    Check(TEXT("opening perft 1-4"),Perft(P,1)==20 && Perft(P,2)==400 && Perft(P,3)==8902 && Perft(P,4)==197281);
    Check(TEXT("kiwipete FEN valid"),P.LoadFen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"));
    Check(TEXT("castling and pins perft 1-3"),Perft(P,1)==48 && Perft(P,2)==2039 && Perft(P,3)==97862);
    Check(TEXT("endgame FEN valid"),P.LoadFen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"));
    Check(TEXT("endgame en passant perft 1-3"),Perft(P,1)==14 && Perft(P,2)==191 && Perft(P,3)==2812);
    Check(TEXT("pinned en passant forbidden"),P.LoadFen("k3r3/8/8/3pP3/8/8/8/4K3 w - d6 0 1") && !Has(P,"e5d6"));
    Check(TEXT("en passant legal when king safe"),P.LoadFen("k7/8/8/3pP3/8/8/8/4K3 w - d6 0 1") && Has(P,"e5d6"));
    Check(TEXT("four promotion choices"),P.LoadFen("7k/P7/8/8/8/8/8/7K w - - 0 1") && Has(P,"a7a8q") && Has(P,"a7a8r") && Has(P,"a7a8b") && Has(P,"a7a8n"));
    Check(TEXT("castling through attacked square forbidden"),P.LoadFen("4kr2/8/8/8/8/8/8/4K2R w K - 0 1") && !Has(P,"e1g1"));
    Game G;const auto Initial=G.At.Fen();Check(TEXT("illegal move leaves position unchanged"),!G.Play("e2e5") && G.At.Fen()==Initial);
    Check(TEXT("fools mate detected"),G.Play("f2f3") && G.Play("e7e5") && G.Play("g2g4") && G.Play("d8h4") && G.Status()=="checkmate" && !G.Play("e2e4"));
    Check(TEXT("undo checkmate restores play"),G.Undo() && !G.Finished());
    G=Game();bool Replay=true;for(int J=0;J<2;++J)for(const char* M:{"g1f3","g8f6","f3g1","f6g8"})Replay&=G.Play(M);
    Check(TEXT("third repetition claim only"),Replay && G.Repetitions()==3 && G.CanClaimDraw() && !G.Finished());
    Game Claim=G;Check(TEXT("draw claim persisted in state"),Claim.ClaimDraw() && Claim.Status()=="claimed" && !Claim.Play("e2e4"));
    for(int J=0;J<2;++J)for(const char* M:{"g1f3","g8f6","f3g1","f6g8"})Replay&=G.Play(M);
    Check(TEXT("fifth repetition automatic"),Replay && G.Status()=="repetition");
    Check(TEXT("stalemate"),P.LoadFen("7k/5K2/6Q1/8/8/8/8/8 b - - 0 1") && P.Legal().empty() && !P.Check(-1));
    Check(TEXT("bare kings material draw"),P.LoadFen("7k/8/8/8/8/8/8/K7 w - - 0 1") && P.InsufficientMaterial());
    Check(TEXT("bishop versus knight not falsely drawn"),P.LoadFen("7k/6n1/8/8/8/8/B7/K7 w - - 0 1") && !P.InsufficientMaterial());
    FString Text,Error;EWWorkshop::Egg E;EWWorkshop::Concept C;
    const FString Root=FPaths::ProjectDir()/TEXT("WorldWorkshop");
    Check(TEXT("read reference egg"),EWWorkshop::ReadBoundedFile(Root/TEXT("Eggs/white-city.egg.json"),Text,Error) && EWWorkshop::ReadEgg(Text,E,Error));
    const auto A=EW::GenerateChunk(E.World,E.Arrival),B=EW::GenerateChunk(E.World,E.Arrival);
    Check(TEXT("egg reproduces reference world digest"),E.World.Code()==EW::WorldDescriptor::ReferenceWorld().Code() && !A.bFallback && A.Digest()==B.Digest());
    FString Bad=Text.Replace(TEXT("city82"),TEXT("future99"));Check(TEXT("unsupported generator content rejected atomically"),!EWWorkshop::ReadEgg(Bad,E,Error) && E.World.Code()==A.World.Code());
    Bad=Text.Replace(TEXT("\"0\""),TEXT("\"9007199254740992\""));Check(TEXT("coordinate overflow rejected"),!EWWorkshop::ReadEgg(Bad,E,Error));
    Check(TEXT("read alternative egg"),EWWorkshop::ReadBoundedFile(Root/TEXT("Eggs/another-sky.egg.json"),Text,Error) && EWWorkshop::ReadEgg(Text,E,Error));
    const auto Other=EW::GenerateChunk(E.World,E.Arrival);Check(TEXT("different egg gives distinct valid world"),Other.Digest()!=A.Digest() && !Other.bFallback && Other.Validate(Error));
    Check(TEXT("read concept package"),EWWorkshop::ReadBoundedFile(Root/TEXT("Concepts/chess-garden.concept.json"),Text,Error) && EWWorkshop::ReadConcept(Text,C,Error));
    Bad=Text.Replace(TEXT("tabletop.v1"),TEXT("native.dll"));Check(TEXT("unknown executable capability rejected"),!EWWorkshop::ReadConcept(Bad,C,Error));
    Bad=Text.Replace(TEXT("160"),TEXT("100000"));Check(TEXT("oversized geometry rejected"),!EWWorkshop::ReadConcept(Bad,C,Error));
    Check(TEXT("path traversal and ADS rejected"),!EWWorkshop::SafeFilename(TEXT("../evil.egg.json"),TEXT(".egg.json")) && !EWWorkshop::SafeFilename(TEXT("bad:stream.egg.json"),TEXT(".egg.json")));
    FTransform Sky;Check(TEXT("sky theatre exists only in reference"),EWSkyTheatrePlan::Frame(A,Sky) && !EWSkyTheatrePlan::Frame(Other,Sky));
    EWSkyTheatrePlan::Frame(A,Sky);double Highest=0;for(const auto& F:A.CityFloors)if(F.bRoof)Highest=FMath::Max(Highest,F.Transform.GetLocation().Z);
    Check(TEXT("sky theatre above highest original floor"),Sky.GetLocation().Z>Highest);
    bool Access=false;for(const auto& L:A.Lifts)Access|=L.Stops.Last().Label.Contains(TEXT("天空シアター"));Check(TEXT("highest theatre added to actual lift"),Access);
    Check(TEXT("larger indoor and outdoor screen"),EWCinemaPlan::ScreenWidth==1850 && EWSkyTheatrePlan::Width==4000 && EWSkyTheatrePlan::Seats().Num()==32);
    Check(TEXT("outdoor audio hard boundary"),EWSkyTheatrePlan::AudioGain(FVector(0,0,100))>0 && EWSkyTheatrePlan::AudioGain(FVector(2400,0,100))==0 && EWSkyTheatrePlan::AudioGain(FVector(0,0,-1))==0);
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Failed==0);O->SetArrayField(TEXT("checks"),Checks);
    O->SetStringField(TEXT("rules_reference"),TEXT("https://handbook.fide.com/chapter/e012023"));FString Out;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Out));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);FFileHelper::SaveStringToFile(Out,*Report);return Failed?1:0;
}
