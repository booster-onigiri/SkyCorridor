#include "EWMemoryPlan.h"
#include "EWSky92Plan.h"
#include "EWExplorationPlan.h"
#include "EWHotelPlan.h"
#include "EWOuterWater.h"
#include "EWSkyTheatrePlan.h"
#include "EWAeroYachtPlan.h"
#include "EWSkyrailPlan.h"

TArray<FEWMemoryPlace> EWMemoryPlan::Build()
{
    const auto W=EW::WorldDescriptor::ReferenceWorld();
    const auto City=EW::GenerateChunk(W,{0,0});
    TArray<FEWMemoryPlace> Out;
    auto Add=[&](FString Id,FString Area,FString Title,FString Moment,FString Record,FString Direction,EW::ChunkCoord C,FVector V)
    {
        FEWMemoryPlace A;A.Place.WorldCode=W.Code();A.Place.Coord=C;A.Place.Id=TEXT("memory89/")+Id;
        A.Place.Name=Title;A.Place.Kind=0;A.Place.LocalPosition=V;A.Area=Area;A.Moment=Moment;A.Record=Record;A.Direction=Direction;A.Approach=V-FVector(550,0,12);Out.Add(A);
    };
    Add(TEXT("water"),TEXT("時計広場"),TEXT("冷たい水"),TEXT("水が冷たい、と誰かが笑った。"),
        TEXT("流れに指をひたしている。隣の誰かにも勧めたらしい。\n返事のかたちは残らず、水面だけが、ふたりぶん揺れた。"),
        TEXT("時計広場の水辺。到着した回廊を、時計に向かって少し進む。"),{0,0},FVector(6800,5550,City.Hub.Z+12));
    const TCHAR* Titles[]={TEXT("冷めるまで"),TEXT("まだ、ここに"),TEXT("栞のかわり"),TEXT("遠い橋の名前"),TEXT("額の外の光"),TEXT("新しい葉")};
    const TCHAR* Moments[]={TEXT("向かいのカップには、まだ触れていない。"),TEXT("出発の時間を、もう一度たしかめる。"),TEXT("読みかけの頁を、細い指で押さえている。"),TEXT("地図にない橋を、ふたりで指さした。"),TEXT("絵より先に、床の光を見ていた。"),TEXT("小さな葉を見つけ、誰かを呼びかけた。")};
    const TCHAR* Records[]={
        TEXT("カップを手のひらで包む。もう一つは向かいの席に。\n飲み終わるまでには、来ると思っていたのだろう。"),
        TEXT("椅子の端に腰かけて、かばんを足元へ寄せる。\n列車が通るたびに顔を上げ、また、窓の外へ目を戻した。"),
        TEXT("薄い紙片が見つからず、糸を一本はさんだ。\n明日も同じ頁から。そのつもりで、本を閉じた。"),
        TEXT("橋の名前は、ふたりで違っていた。\nどちらが正しいとも決めず、次の休みに行こうと話した。"),
        TEXT("床を横切る四角い光が、少しずつ傾いていく。\n絵を見る時間より長く、その端を目で追っていた。"),
        TEXT("昨日まではなかった葉を見つける。\nかがんで水を足し、誰かに知らせようと振り返った。")};
    for(int32 I=0;I<6;++I)if(const auto* Room=EWExplorationPlan::Find(City,I))
    {
        Add(FString::Printf(TEXT("room%d"),I),EWExplorationPlan::Name(I),Titles[I],Moments[I],Records[I],EWExplorationPlan::Directions(I),{0,0},Room->Frame.TransformPosition(FVector(-400,-1450,12)));
        Out.Last().Approach=Room->Frame.TransformPosition(FVector(0,-2100,0));
    }
    if(const auto* Room=EWHotelPlan::Find(City,0))
    {
        Add(TEXT("hotel"),TEXT("雲上ホテル 101"),TEXT("明日の窓"),TEXT("カーテンを少しだけ開けておく。"),
            TEXT("明日の朝、光で目が覚めるように。\n窓辺に置いた一脚の椅子は、街のほうを向いていた。"),
            TEXT("時計広場駅の昇降機で上層線へ。雲上ホテル駅で降り、客室階の 101 凪の和邸へ。"),{0,0},Room->Frame.TransformPosition(FVector(0,-100,12)));
        Out.Last().Approach=Room->Frame.TransformPosition(FVector(0,-740,0));
    }
    Add(TEXT("platform"),TEXT("時計広場駅"),TEXT("一本あとで"),TEXT("開いた扉の前で、足を止めた。"),
        TEXT("列車の扉が閉まる。乗り遅れたのではないらしい。\n誰かの来る回廊を見ながら、次を待つことにした。"),
        TEXT("時計広場駅の水都線ホーム。東側の昇降機から上がる。"),{0,0},EWSkyrailPlan::Stop(0)+FVector(600,460,12));
    const auto Water=EW::GenerateChunk(W,{3,0});
    const auto* Sanctuary=Water.Places.FindByPredicate([](const auto& P){return P.Kind==15;});
    const auto WaterEntry=EWOuterWater::Entrance(W);
    Add(TEXT("sanctuary"),TEXT("白塔の水都"),TEXT("水音の数え方"),TEXT("滝の音に、別の音を探している。"),
        TEXT("同じ水音にも、いくつも層があると言った。\n目を閉じて教えようとした、その先は残っていない。"),
        TEXT("時計広場駅の水都線で終点へ。昇降機で水都に降り、中央の聖堂側の広場へ。"),{3,0},Sanctuary?Sanctuary->LocalPosition-FVector(0,0,78):WaterEntry.LocalPosition-FVector(0,0,78));
    FTransform ScreenFrame;EWSkyTheatrePlan::Frame(City,ScreenFrame);
    Add(TEXT("theatre"),TEXT("天空シアター"),TEXT("上映が終わっても"),TEXT("画面が暗くなったあとも、座っていた。"),
        TEXT("もう一度見たい場面を、言葉にしようとしている。\nでも、先に風が吹いた。しばらくは、それだけでよかった。"),
        TEXT("上層線の天空シアター駅から昇降機で屋上へ。客席の外側の回廊。"),{0,0},ScreenFrame.TransformPosition(FVector(2200,-500,12)));
    Out.Last().Approach=ScreenFrame.TransformPosition(FVector(2200,50,0));
    const auto T=EWAeroYachtPlan::Design().Terminal-FVector(EW::ChunkSize,0,0);
    Add(TEXT("port"),TEXT("アウレリア空中港"),TEXT("見送る手"),TEXT("見えなくなるまで、手を下ろさなかった。"),
        TEXT("船の窓は、もう小さな点になっている。\nそれでも手を振り、やがて、手すりにそっと置いた。"),
        TEXT("上層線のアウレリア空中港駅から展望桟橋へ。乗船口の手前。"),{1,0},T+FVector(800,650,12));
    const TCHAR* NewTitles[]={TEXT("最後の包み"),TEXT("風にめくれた頁"),TEXT("城の食卓")};
    const TCHAR* NewMoments[]={TEXT("包みの端を、もう一度折り直す。"),TEXT("頁が戻るたび、同じ行を読んでいる。"),TEXT("向かいの椅子を、少し引いた。")};
    const TCHAR* NewRecords[]={TEXT("店先で、小さな包みを渡そうとしていた。\n贈る相手のことを聞かれたのか、かすかに肩が動いた。"),
        TEXT("石の回廊には、風の通り道がある。\n本を閉じずに手を離し、遠くから来る足音を待っていた。"),
        TEXT("大きな城の片隅に、ふたりぶんの席。\n一つの皿を真ん中へ寄せる。その小さな距離だけが残った。")};
    for(int I=0;I<3;++I)
    {
        const auto F=EWSky92Plan::Frame(W,I);
        const FVector Local=I==0?FVector(-600,-1320,12):I==1?FVector(-650,350,12):FVector(-400,1350,12);
        Add(FString::Printf(TEXT("sky92-%d"),I),EWSky92Plan::Name(I),NewTitles[I],NewMoments[I],NewRecords[I],EWSky92Plan::Directions(I),EWSky92Plan::Coord(I),F.TransformPosition(Local));
        Out.Last().Approach=F.TransformPosition(I==0?FVector(0,-1730,0):Local+FVector(0,-500,-12));
    }
    return Out;
}
