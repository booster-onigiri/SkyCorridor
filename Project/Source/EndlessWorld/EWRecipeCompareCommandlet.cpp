#include "EWRecipeCompareCommandlet.h"
#include "EWWorld.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
UEWRecipeCompareCommandlet::UEWRecipeCompareCommandlet()
{IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;ShowErrorCount=true;}
int32 UEWRecipeCompareCommandlet::Main(const FString& Params)
{
    FString Output;int32 Selected=-1;
    if(!FParse::Value(*Params,TEXT("EWReport="),Output) || IFileManager::Get().FileExists(*Output))return 2;
    FParse::Value(*Params,TEXT("EWSample="),Selected);
    auto Vec=[](FVector V){TArray<TSharedPtr<FJsonValue>> A;for(double X:{V.X,V.Y,V.Z})A.Add(MakeShared<FJsonValueNumber>(X));return A;};
    auto Transform=[&](const FTransform& T){auto O=MakeShared<FJsonObject>();O->SetArrayField(TEXT("position"),Vec(T.GetLocation()));O->SetArrayField(TEXT("scale"),Vec(T.GetScale3D()));O->SetArrayField(TEXT("euler"),Vec(T.GetRotation().Euler()));const FQuat Q=T.GetRotation();TArray<TSharedPtr<FJsonValue>> A;for(double X:{Q.X,Q.Y,Q.Z,Q.W})A.Add(MakeShared<FJsonValueNumber>(X));O->SetArrayField(TEXT("quaternion"),A);return O;};
    TArray<TSharedPtr<FJsonValue>> Rows;FString Aggregate;
    for(int32 Seed=0;Seed<100;++Seed)
    {
        EW::WorldDescriptor W{EW::HashText(FString::Printf(TEXT("endless-core-audit-%d"),Seed)).Left(32)};
        for(int32 I=0;I<100;++I)
        {
            const int32 Index=Seed*100+I;if(Selected>=0 && Index!=Selected)continue;
            EW::ChunkCoord C;
            if(I==0)C={0,0};else if(I==1)C={-1,-1};else if(I==2)C={-100000000000LL,100000000000LL};
            else if(I==3)C={EW::CoordLimit-1,-EW::CoordLimit+1};
            else C={int64(W.Number("audit-x",{I})%20001)-10000,int64(W.Number("audit-y",{I})%20001)-10000};
            const auto R=EW::GenerateChunk(W,C);auto O=MakeShared<FJsonObject>();
            O->SetNumberField(TEXT("index"),Index);O->SetStringField(TEXT("world"),W.Code());O->SetStringField(TEXT("coord"),C.Text());
            O->SetStringField(TEXT("digest"),R.Digest());Aggregate+=R.Digest();O->SetNumberField(TEXT("region"),int32(R.Region));
            O->SetNumberField(TEXT("parts_count"),R.Parts.Num());O->SetNumberField(TEXT("colliders_count"),R.Colliders.Num());
            if(Selected>=0)
            {
                TArray<TSharedPtr<FJsonValue>> Parts,Boxes,Paths,Places;
                for(const auto& P:R.Parts){auto B=Transform(P.Transform);B->SetStringField(TEXT("mesh"),P.Mesh.ToString());B->SetNumberField(TEXT("detail"),P.Detail);Parts.Add(MakeShared<FJsonValueObject>(B));}
                for(const auto& P:R.Colliders){auto B=Transform(P.Transform);B->SetArrayField(TEXT("extent"),Vec(P.Extent));B->SetBoolField(TEXT("floor"),P.bWalkSurface);Boxes.Add(MakeShared<FJsonValueObject>(B));}
                for(const auto& P:R.Paths){auto B=MakeShared<FJsonObject>();B->SetArrayField(TEXT("a"),Vec(P.A));B->SetArrayField(TEXT("b"),Vec(P.B));B->SetNumberField(TEXT("width"),P.Width);Paths.Add(MakeShared<FJsonValueObject>(B));}
                for(const auto& P:R.Places){auto B=MakeShared<FJsonObject>();B->SetStringField(TEXT("id"),P.Id);B->SetNumberField(TEXT("kind"),P.Kind);B->SetArrayField(TEXT("position"),Vec(P.LocalPosition));Places.Add(MakeShared<FJsonValueObject>(B));}
                O->SetArrayField(TEXT("hub"),Vec(R.Hub));O->SetArrayField(TEXT("parts"),Parts);O->SetArrayField(TEXT("colliders"),Boxes);O->SetArrayField(TEXT("paths"),Paths);O->SetArrayField(TEXT("places"),Places);
            }
            Rows.Add(MakeShared<FJsonValueObject>(O));
        }
    }
    auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("success"),true);Report->SetStringField(TEXT("scope"),TEXT("Diagnostic export, not gameplay acceptance"));Report->SetStringField(TEXT("aggregate_digest"),EW::HashText(Aggregate));Report->SetArrayField(TEXT("recipes"),Rows);
    FString Text;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));return FFileHelper::SaveStringToFile(Text,*Output,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)?0:1;
}
