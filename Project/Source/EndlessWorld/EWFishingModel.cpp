#include "EWFishingModel.h"

namespace EWFishing
{
const TArray<FSpecies>& Species()
{
    static const TArray<FSpecies> Fish={
        {TEXT("cloud-minnow"),TEXT("ソラメダカ"),TEXT("白い街の水路に群れる小さな魚。水面の雲を追いかける。"),7,15,28,3,8,{.58f,.83f,.87f,1},.68},
        {TEXT("silver-bream"),TEXT("銀雲ブナ"),TEXT("雲が流れるように、ゆっくりと水庭を巡る。"),3,3,18,12,32,{.73f,.81f,.87f,1},1.15},
        {TEXT("clock-koi"),TEXT("時計ゴイ"),TEXT("時計広場の主。金色の尾を見つけると良い一日になる。"),1,6,9,25,65,{.90f,.49f,.19f,1},1.1},
        {TEXT("petal-fish"),TEXT("花びらウオ"),TEXT("朝の水庭に浮かぶ花びらのような、淡い桃色の魚。"),2,1,13,6,17,{.95f,.55f,.59f,1},1.35},
        {TEXT("glass-trout"),TEXT("ガラスマス"),TEXT("滝の冷たい水を好む。青く澄んだ鱗が陽にきらめく。"),2,3,14,18,44,{.31f,.73f,.83f,1},.74},
        {TEXT("canal-perch"),TEXT("水路スズキ"),TEXT("南の流れで育つ力強い魚。石橋の影が休み場所。"),4,7,18,18,46,{.38f,.65f,.55f,1},.92},
        {TEXT("sunset-carp"),TEXT("夕焼けコイ"),TEXT("夕方だけ、朱色のひれが水面を染める。"),5,4,12,20,55,{.95f,.31f,.17f,1},1.18},
        {TEXT("lantern-tetra"),TEXT("灯りテトラ"),TEXT("街灯がともるころ現れる。小さな金の粒のよう。"),7,12,19,4,12,{1.f,.74f,.25f,1},.85},
        {TEXT("moon-angel"),TEXT("月影エンゼル"),TEXT("夜の水庭にひそむ銀色の魚。長いひれが月を映す。"),2,8,8,10,25,{.72f,.73f,.99f,1},1.65},
        {TEXT("star-catfish"),TEXT("星屑ナマズ"),TEXT("静かな南水路の底にいる。星が出るころ活動を始める。"),4,8,12,25,70,{.40f,.43f,.68f,1},.68},
        {TEXT("rain-ribbon"),TEXT("雨糸ウナギ"),TEXT("朝夕の細い流れを縫うように泳ぐ、リボンのような魚。"),6,5,6,35,85,{.35f,.58f,.67f,1},.32},
        {TEXT("aurora-fish"),TEXT("オーロラフィッシュ"),TEXT("夜明けと深夜、ごくまれに水面を虹色に照らす。"),7,9,2,20,48,{.45f,.90f,.78f,1},1.3}
    };
    return Fish;
}
const TArray<FSpot>& Spots()
{
    static const TArray<FSpot> Values={
        {TEXT("clock-quay"),TEXT("時計広場の水辺"),TEXT("時計広場の南側。低い手すりに沿って、釣りの看板へ。"),{0,0},{5900,5200,2338},{5900,4710,1955},-90},
        {TEXT("north-garden"),TEXT("北の水庭"),TEXT("北の昇降機で水庭の階へ。滝を眺める渡り廊下の先。"),{0,0},{7000,12100,4138},{6600,13150,3855},111},
        {TEXT("south-canal"),TEXT("南の水路"),TEXT("時計広場から南の回廊をまっすぐ。南端の滝の手前にある釣りの看板へ。"),{0,0},{6653.5,182.513,2322.455},{7070,-900,1955},-69}
    };
    return Values;
}
const FSpecies* Find(const FString& Id)
{return Species().FindByPredicate([&](const FSpecies& S){return S.Id==Id;});}
uint8 HourBand(double Hour)
{
    if(!FMath::IsFinite(Hour) || Hour<0 || Hour>=24)return 0;
    return Hour>=5 && Hour<9?1:Hour>=9 && Hour<17?2:Hour>=17 && Hour<21?4:8;
}
FString TimeDescription(uint8 Bands)
{
    if(Bands==15)return TEXT("いつでも");TArray<FString> Text;
    if(Bands&1)Text.Add(TEXT("朝"));if(Bands&2)Text.Add(TEXT("昼"));if(Bands&4)Text.Add(TEXT("夕"));if(Bands&8)Text.Add(TEXT("夜"));
    return FString::Join(Text,TEXT("・"));
}
FString SiteDescription(uint8 Sites)
{
    if(Sites==7)return TEXT("すべての水辺");TArray<FString> Text;
    for(int32 I=0;I<Spots().Num();++I)if(Sites&(1<<I))Text.Add(Spots()[I].Name);
    return FString::Join(Text,TEXT(" / "));
}
bool Available(const FSpecies& Fish,int32 Site,double Hour)
{return Spots().IsValidIndex(Site) && (Fish.Sites&(1<<Site)) && (Fish.Hours&HourBand(Hour));}
bool FCatch::Valid() const
{
    FGuid Guid;FDateTime Date;const auto* S=Find(Species);
    return S && FGuid::ParseExact(Id,EGuidFormats::DigitsWithHyphens,Guid) && Guid.IsValid() &&
        FDateTime::ParseIso8601(*UTC,Date) && Available(*S,Site,Hour) && FMath::IsFinite(Length) && Length>=S->Minimum && Length<=S->Maximum;
}
FCatch Roll(int32 Site,double Hour,FRandomStream& Random,const FString& Id)
{
    FCatch C;int32 Total=0;for(const auto& S:Species())if(Available(S,Site,Hour))Total+=S.Weight;
    if(Total<=0)return C;int32 Pick=Random.RandRange(1,Total);
    for(const auto& S:Species())if(Available(S,Site,Hour) && (Pick-=S.Weight)<=0)
    {
        C.Id=Id;C.Species=S.Id;C.Site=Site;C.Hour=Hour;C.UTC=FDateTime::UtcNow().ToIso8601();
        // Larger specimens are less common; every size stays within the catalogue.
        C.Length=FMath::RoundToDouble(FMath::Lerp(S.Minimum,S.Maximum,FMath::Pow(double(Random.FRand()),1.8))*10.)/10.;return C;
    }
    return C;
}
bool FCast::Begin(int32 Site,double Hour,double Now,bool Patient,FRandomStream& Random)
{
    if(Active() || State==EPhase::Landed || !FMath::IsFinite(Now) || Now<0)return false;
    const auto NewFish=Roll(Site,Hour,Random,FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
    if(!NewFish.Valid())return false;Fish=NewFish;bPatient=Patient;BiteAt=Now+Random.FRandRange(5.f,11.f);
    Deadline=BiteAt+12;LandAt=0;State=EPhase::Waiting;return true;
}
void FCast::Tick(double Now)
{
    if(!FMath::IsFinite(Now))return;
    if(State==EPhase::Waiting && Now>=BiteAt)State=EPhase::Bite;
    if(State==EPhase::Bite && !bPatient && Now>Deadline)State=EPhase::Escaped;
    if(State==EPhase::Reeling && Now>=LandAt)State=EPhase::Landed;
}
bool FCast::Press(double Now)
{
    Tick(Now);if(State!=EPhase::Bite || !FMath::IsFinite(Now) || Now<BiteAt)return false;
    State=EPhase::Reeling;LandAt=Now+2.8;return true;
}
void FCast::Cancel(){State=EPhase::Idle;Fish={};BiteAt=Deadline=LandAt=0;}
double FCast::Progress(double Now) const
{return State==EPhase::Reeling?FMath::Clamp(1.-(LandAt-Now)/2.8,0.,1.):0.;}
}
