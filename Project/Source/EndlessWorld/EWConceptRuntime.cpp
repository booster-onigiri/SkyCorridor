#include "EWConceptRuntime.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWSocialSession.h"
#include "EWCinemaSession.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

AEWConceptRuntime::AEWConceptRuntime()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ConceptAnchor")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> C(TEXT("/Engine/BasicShapes/Cube.Cube")),S(TEXT("/Engine/BasicShapes/Sphere.Sphere")),Y(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")),K(TEXT("/Engine/BasicShapes/Cone.Cone"));
    Cube=C.Object;Sphere=S.Object;Cylinder=Y.Object;Cone=K.Object;
    Body=CreateDefaultSubobject<UBoxComponent>(TEXT("TableCollision"));Body->SetupAttachment(RootComponent);
    Body->SetCollisionResponseToAllChannels(ECR_Block);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);Body->SetCanEverAffectNavigation(false);
}
FString AEWConceptRuntime::Directory() const
{const auto* G=GetGameInstance<UEWGameInstance>();return G && G->Store()?G->Store()->Root()/TEXT("WorldWorkshop"):FString();}
void AEWConceptRuntime::BeginPlay()
{
    Super::BeginPlay();Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_LifeSurface.M_LifeSurface"));
    if(Directory().IsEmpty())return;
    const FString Source=FPaths::ProjectDir()/TEXT("WorldWorkshop");
    for(const TCHAR* Kind:{TEXT("Eggs"),TEXT("Concepts")})
    {
        IFileManager::Get().MakeDirectory(*(Directory()/Kind),true);TArray<FString> Names;
        IFileManager::Get().FindFiles(Names,*(Source/Kind/TEXT("*.json")),true,false);
        for(const auto& N:Names)if(!IFileManager::Get().FileExists(*(Directory()/Kind/N)))IFileManager::Get().Copy(*(Directory()/Kind/N),*(Source/Kind/N),false);
    }
}
TArray<FString> AEWConceptRuntime::Files(bool Eggs) const
{
    TArray<FString> Names;IFileManager::Get().FindFiles(Names,*(Directory()/(Eggs?TEXT("Eggs"):TEXT("Concepts"))/TEXT("*.json")),true,false);
    Names.RemoveAll([Eggs](const FString& N){return !EWWorkshop::SafeFilename(N,Eggs?TEXT(".egg.json"):TEXT(".concept.json"));});Names.Sort();if(Names.Num()>64)Names.SetNum(64);return Names;
}
bool AEWConceptRuntime::Tell(bool OK,const FString& Text)
{Message=Text;if(auto* G=GetGameInstance<UEWGameInstance>())G->Notify(Text);return OK;}
bool AEWConceptRuntime::LocalOnly()
{
    const auto* G=GetGameInstance<UEWGameInstance>();
    if(!G || !G->Manager || G->Manager->IsTravelling())return Tell(false,TEXT("世界の読込みが終わるまでお待ちください。"));
    if((G->SocialSession && (G->SocialSession->Active() || G->SocialSession->Network().Busy())) || (G->CinemaSession && G->CinemaSession->Connected()))
        return Tell(false,TEXT("この実証は一人用の世界で利用できます。共有の街から退出してから開いてください。"));
    return true;
}
bool AEWConceptRuntime::LoadEgg(const FString& File)
{
    if(!LocalOnly())return false;FString Text,Error;EWWorkshop::Egg Egg;
    if(!EWWorkshop::SafeFilename(File,TEXT(".egg.json")) || !EWWorkshop::ReadBoundedFile(Directory()/TEXT("Eggs")/File,Text,Error) || !EWWorkshop::ReadEgg(Text,Egg,Error))return Tell(false,Error.IsEmpty()?TEXT("卵のファイル名が無効です。"):Error);
    const auto R=EW::GenerateChunk(Egg.World,Egg.Arrival);
    if(R.bFallback || !R.Validate(Error))return Tell(false,TEXT("生成した世界の通路検査に失敗しました。"));
    EW::PlaceBookmark P;P.WorldCode=Egg.World.Code();P.Coord=Egg.Arrival;P.Name=Egg.Title;P.Id=EW::HashText(TEXT("egg|")+Egg.Id).Left(32);
    P.LocalPosition=R.SafePosition(R.Hub+FVector(0,-1250,100));P.Yaw=90;
    auto* G=GetGameInstance<UEWGameInstance>();G->Visit(P);
    if(G->Manager->Descriptor().Code()!=P.WorldCode)return Tell(false,TEXT("保存または移動が完了しなかったため、元の世界を維持しました。"));
    Remove();LoadedEgg=Egg.Title;LoadedDigest=R.Digest();return Tell(true,Egg.Title+TEXT("を卵から読み込みました。"));
}
bool AEWConceptRuntime::Placement(double Size,FVector& At,double& Yaw) const
{
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(!P || P->IsSeated() || P->IsStreamingHeld())return false;
    const auto* PC=UGameplayStatics::GetPlayerController(this,0);Yaw=PC?PC->GetControlRotation().Yaw:P->GetActorRotation().Yaw;
    const FRotator R(0,Yaw,0);At=P->GetActorLocation()+R.Vector()*(Size*.5+190);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(ConceptPlacement),false,this);Q.AddIgnoredActor(P);
    FHitResult Hit;
    if(!GetWorld()->LineTraceSingleByChannel(Hit,At+FVector(0,0,40),At-FVector(0,0,180),ECC_Visibility,Q) || !Hit.Component.IsValid() || !Hit.Component->ComponentHasTag(TEXT("EWFloor")) || Hit.ImpactNormal.Z<.95)return false;
    At.Z=Hit.ImpactPoint.Z+1;
    // Keep one metre of walkable clearance on all sides, including corners.
    for(int X=-1;X<=1;++X)for(int Y=-1;Y<=1;++Y)
    {
        const FVector V=At+R.RotateVector(FVector(X*(Size*.5+100),Y*(Size*.5+100),0));FHitResult Floor;
        if(!GetWorld()->LineTraceSingleByChannel(Floor,V+FVector(0,0,25),V-FVector(0,0,25),ECC_Visibility,Q) || !Floor.Component.IsValid() || !Floor.Component->ComponentHasTag(TEXT("EWFloor")) || FMath::Abs(Floor.ImpactPoint.Z-At.Z)>8)return false;
    }
    return !GetWorld()->OverlapBlockingTestByChannel(At+FVector(0,0,75),R.Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(FVector(Size*.5+90,Size*.5+90,72)),Q);
}
FString AEWConceptRuntime::MatchPath(const FString& Id) const
{const auto* G=GetGameInstance<UEWGameInstance>();return Directory()/TEXT("State")/ (EW::HashText((G && G->Manager?G->Manager->Descriptor().Code():FString())+TEXT("|")+Id)+TEXT(".json"));}
bool AEWConceptRuntime::ReadMatch(const FString& Id,EWChess::Game& Out)
{
    const FString Path=MatchPath(Id);if(!IFileManager::Get().FileExists(*Path))return true;
    auto Parse=[&](const FString& File,EWChess::Game& Match)
    {
        FString T,E;TSharedPtr<FJsonObject> O;const TArray<TSharedPtr<FJsonValue>>* Moves=nullptr;double V=0;FString Rule;bool Claim=false;
        if(!EWWorkshop::ReadBoundedFile(File,T,E) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T),O) || !O || O->Values.Num()!=4 ||
            !O->TryGetNumberField(TEXT("version"),V) || V!=1 || !O->TryGetStringField(TEXT("rules"),Rule) || Rule!=TEXT("chess.standard.v1") ||
            !O->TryGetBoolField(TEXT("draw_claimed"),Claim) || !O->TryGetArrayField(TEXT("moves"),Moves) || Moves->Num()>3000)return false;
        EWChess::Game NewGame;for(const auto& M:*Moves){FString U;if(!M->TryGetString(U) || U.Len()>5 || !NewGame.Play(TCHAR_TO_UTF8(*U)))return false;}
        if(Claim && !NewGame.ClaimDraw())return false;Match=MoveTemp(NewGame);return true;
    };
    if(Parse(Path,Out))return true;
    if(Parse(Path+TEXT(".backup"),Out)){Tell(true,TEXT("対局の予備保存から復元しました。元のファイルは残しています。"));return true;}
    return Tell(false,TEXT("対局の保存を検証できませんでした。ファイルを残して追加を中止しました。"));
}
bool AEWConceptRuntime::Add(const FString& File)
{
    if(!LocalOnly())return false;if(bActive)return Tell(false,TEXT("現在の卓を撤去してから、次の追加要素を置いてください。"));
    FString Text,Error;EWWorkshop::Concept Next;EWChess::Game Match;FVector At;double Yaw=0;
    if(!EWWorkshop::SafeFilename(File,TEXT(".concept.json")) || !EWWorkshop::ReadBoundedFile(Directory()/TEXT("Concepts")/File,Text,Error) || !EWWorkshop::ReadConcept(Text,Next,Error))return Tell(false,Error.IsEmpty()?TEXT("追加要素のファイル名が無効です。"):Error);
    if(!Cube || !Sphere || !Cylinder || !Cone || !Surface)return Tell(false,TEXT("必要な共通部品が見つかりません。"));
    if(!Placement(Next.Size,At,Yaw))return Tell(false,TEXT("少し広い平らな床へ向いてください。卓の周囲に1 mの通路が必要です。"));
    if(!ReadMatch(Next.Id,Match))return false;
    auto* M=GetGameInstance<UEWGameInstance>()->Manager.Get();Definition=Next;Game=MoveTemp(Match);AnchorChunk=M->PlayerCoord();AnchorLocal=M->ToLocal(AnchorChunk,At);AnchorYaw=Yaw;WorldCode=M->Descriptor().Code();
    SetActorLocationAndRotation(At,FRotator(0,Yaw,0));bActive=true;Build();return Tell(true,Definition.Title+TEXT("を追加しました。卓に近づいて E で遊べます。"));
}
void AEWConceptRuntime::Clear()
{
    for(const auto& M:Shapes)if(M)M->DestroyComponent();for(const auto& M:PieceShapes)if(M)M->DestroyComponent();Shapes.Empty();PieceShapes.Empty();Materials.Empty();
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void AEWConceptRuntime::Remove()
{
    Clear();bActive=false;
    auto* G=GetGameInstance<UEWGameInstance>();if(G && G->Menu()==EEWMenu::Chess)G->SetMenu(EEWMenu::Workshop);
}
UStaticMeshComponent* AEWConceptRuntime::Shape(UStaticMesh* Mesh,FVector P,FVector Size,int32 Material,bool Piece,FRotator Q)
{
    auto* M=NewObject<UStaticMeshComponent>(this);M->SetupAttachment(RootComponent);M->SetMobility(EComponentMobility::Movable);
    M->SetStaticMesh(Mesh);M->SetRelativeLocation(P);M->SetRelativeRotation(Q);M->SetRelativeScale3D(Size/100.);M->SetMaterial(0,Materials[Material]);
    M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetCanEverAffectNavigation(false);M->RegisterComponent();(Piece?PieceShapes:Shapes).Add(M);return M;
}
void AEWConceptRuntime::Build()
{
    Clear();
    for(const auto C:{Definition.Light,Definition.Dark,Definition.Accent}){auto* M=UMaterialInstanceDynamic::Create(Surface,this);M->SetVectorParameterValue(TEXT("Tint"),C);Materials.Add(M);}
    const double Size=Definition.Size,Cell=Size/8;
    Shape(Cube,{0,0,78},{Size+14,Size+14,8},2);Shape(Cube,{0,0,69},{Size+4,Size+4,10},1);
    for(int X:{-1,1})for(int Y:{-1,1}){Shape(Cylinder,{X*Size*.36,Y*Size*.36,34},{12,12,68},1);Shape(Cylinder,{X*Size*.36,Y*Size*.36,5},{17,17,8},2);}
    for(int Y=0;Y<8;++Y)for(int X=0;X<8;++X)Shape(Cube,{(X-3.5)*Cell,(Y-3.5)*Cell,82.6},{Cell-.2,Cell-.2,1.2},(X+Y)%2?0:1);
    Body->SetRelativeLocation(FVector(0,0,41));Body->SetBoxExtent(FVector(Size*.5+7,Size*.5+7,41));Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    UpdatePieces();
}
void AEWConceptRuntime::UpdatePieces()
{
    for(const auto& M:PieceShapes)if(M)M->DestroyComponent();PieceShapes.Empty();const double Cell=Definition.Size/8;
    for(int I=0;I<64;++I)
    {
        const int P=Game.At.Board[I],K=FMath::Abs(P);if(!P)continue;
        const int Mat=P>0?0:1;const FVector Base((I%8-3.5)*Cell,(I/8-3.5)*Cell,83.2);
        auto Part=[&](UStaticMesh* Mesh,FVector Pos,FVector Ext,int Material,FRotator Q=FRotator::ZeroRotator){return Shape(Mesh,Base+Pos*Cell,Ext*Cell,Material,true,Q);};
        const double H=K==1?.64:K==6?1.28:K==5?1.10:K==4?.82:1.;
        Part(Cylinder,{0,0,.08},{.64,.64,.16},Mat);Part(Cylinder,{0,0,.19},{.53,.53,.08},2);
        Part(Cone,{0,0,H*.50},{.44,.44,H*.70},Mat);Part(Cylinder,{0,0,H*.77},{.42,.42,.10},Mat);
        if(K==1)Part(Sphere,{0,0,H},{.40,.40,.40},Mat);
        if(K==2){Part(Cube,{0,.08,H},{.28,.53,.48},Mat,FRotator(-25,0,0));Part(Cube,{0,-.11,H+.15},{.18,.12,.15},2);}
        if(K==3){Part(Cone,{0,0,H},{.45,.45,.52},Mat);Part(Sphere,{0,0,H+.28},{.12,.12,.12},2);}
        if(K==4){Part(Cylinder,{0,0,H},{.59,.59,.19},Mat);for(int J=0;J<4;++J){double A=J*PI/2;Part(Cube,{.20*FMath::Cos(A),.20*FMath::Sin(A),H+.17},{.17,.17,.20},Mat);}}
        if(K==5){Part(Sphere,{0,0,H},{.38,.38,.34},Mat);for(int J=0;J<5;++J){double A=J*2*PI/5;Part(Sphere,{.20*FMath::Cos(A),.20*FMath::Sin(A),H+.18},{.13,.13,.13},2);}}
        if(K==6){Part(Sphere,{0,0,H-.06},{.32,.32,.32},Mat);Part(Cube,{0,0,H+.20},{.13,.13,.45},2);Part(Cube,{0,0,H+.28},{.38,.13,.12},2);}
    }
}
bool AEWConceptRuntime::Nearby() const
{
    const auto* P=UGameplayStatics::GetPlayerCharacter(this,0);return bActive && !IsHidden() && P && FVector::Dist2D(P->GetActorLocation(),GetActorLocation())<Definition.Size*.5+230 && FMath::Abs(P->GetActorLocation().Z-GetActorLocation().Z)<180;
}
bool AEWConceptRuntime::Interact(){if(!Nearby())return false;GetGameInstance<UEWGameInstance>()->SetMenu(EEWMenu::Chess);return true;}
void AEWConceptRuntime::Tick(float Delta)
{
    Super::Tick(Delta);if(!bActive)return;const auto* G=GetGameInstance<UEWGameInstance>();const auto* M=G?G->Manager.Get():nullptr;
    if(!M || M->Descriptor().Code()!=WorldCode || (G->SocialSession && G->SocialSession->Active()) || (G->CinemaSession && G->CinemaSession->Connected())){Remove();return;}
    const bool Ready=M->ReadyAt(AnchorChunk);SetActorHiddenInGame(!Ready);Body->SetCollisionEnabled(Ready?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    if(Ready)SetActorLocationAndRotation(M->ToRender(AnchorChunk,AnchorLocal),FRotator(0,AnchorYaw,0));
}
bool AEWConceptRuntime::Commit(const EWChess::Game& Next)
{
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("version"),1);O->SetStringField(TEXT("rules"),TEXT("chess.standard.v1"));O->SetBoolField(TEXT("draw_claimed"),Next.DrawClaimed);
    TArray<TSharedPtr<FJsonValue>> Moves;for(const auto& M:Next.Moves)Moves.Add(MakeShared<FJsonValueString>(UTF8_TO_TCHAR(M.c_str())));O->SetArrayField(TEXT("moves"),Moves);
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
    if(!EWWorkshop::AtomicWrite(MatchPath(Definition.Id),Text))return Tell(false,TEXT("対局を保存できないため、操作を取り消しました。"));
    Game=Next;UpdatePieces();return true;
}
bool AEWConceptRuntime::Play(const FString& Move)
{if(!bActive || !LocalOnly() || !Nearby())return false;EWChess::Game Next=Game;if(Move.Len()>5 || !Next.Play(TCHAR_TO_UTF8(*Move)))return Tell(false,TEXT("その手は指せません。移動先の候補を選んでください。"));return Commit(Next);}
bool AEWConceptRuntime::Undo(){if(!bActive || !LocalOnly())return false;auto Next=Game;return Next.Undo() && Commit(Next);}
bool AEWConceptRuntime::NewMatch(){return bActive && LocalOnly() && Commit(EWChess::Game());}
bool AEWConceptRuntime::ClaimDraw(){if(!bActive || !LocalOnly())return false;auto Next=Game;return Next.ClaimDraw() && Commit(Next);}
FString AEWConceptRuntime::MatchStatus() const
{
    const auto S=Game.Status();const FString Side=Game.At.Side==1?TEXT("白"):TEXT("黒");
    if(S=="checkmate")return (Game.At.Side==1?FString(TEXT("黒")):FString(TEXT("白")))+TEXT("の勝ち・チェックメイト");
    if(S=="stalemate")return TEXT("引き分け・ステイルメイト");if(S=="material")return TEXT("引き分け・戦力不足");
    if(S=="claimed")return TEXT("引き分けを申請しました");if(S=="repetition")return TEXT("引き分け・同一局面5回");if(S=="seventy-five")return TEXT("引き分け・75手ルール");
    return Side+TEXT("の手番")+(S=="check"?TEXT("・王手"):TEXT(""));
}
TSharedRef<FJsonObject> AEWConceptRuntime::Evidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("active"),bActive);O->SetStringField(TEXT("package"),Definition.Id);O->SetStringField(TEXT("egg"),LoadedEgg);O->SetStringField(TEXT("recipe_digest"),LoadedDigest);
    O->SetStringField(TEXT("fen"),UTF8_TO_TCHAR(Game.At.Fen().c_str()));O->SetNumberField(TEXT("moves"),Game.Moves.size());O->SetNumberField(TEXT("meshes"),Shapes.Num()+PieceShapes.Num());O->SetStringField(TEXT("scope"),TEXT("local-two-sides"));
    O->SetStringField(TEXT("anchor_chunk"),AnchorChunk.Text());O->SetStringField(TEXT("anchor_local"),AnchorLocal.ToString());O->SetStringField(TEXT("message"),Message);return O;
}
FString AEWConceptRuntime::WorkshopAction(const FString& Action,const FString& Value)
{
    bool OK=false;if(Action==TEXT("egg"))OK=LoadEgg(Value);else if(Action==TEXT("add"))OK=Add(Value);else if(Action==TEXT("remove")){Remove();OK=true;}
    else if(Action==TEXT("move"))OK=Play(Value);else if(Action==TEXT("undo"))OK=Undo();else if(Action==TEXT("new"))OK=NewMatch();else if(Action==TEXT("open")){GetGameInstance<UEWGameInstance>()->SetMenu(EEWMenu::Workshop);OK=true;}else if(Action==TEXT("inspect"))OK=true;
    auto O=Evidence();O->SetBoolField(TEXT("applied"),OK);FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));return Text;
}
