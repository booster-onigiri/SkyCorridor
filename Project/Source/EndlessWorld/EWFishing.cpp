#include "EWFishing.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWDayCycle.h"
#include "EWSocialSession.h"
#include "EWSocialProtocol.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"

AEWFishing::AEWFishing()
{
    PrimaryActorTick.bCanEverTick=true;
    Root=CreateDefaultSubobject<USceneComponent>(TEXT("FishingRoot"));SetRootComponent(Root);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> C(TEXT("/Engine/BasicShapes/Cube.Cube")),S(TEXT("/Engine/BasicShapes/Sphere.Sphere")),
        Y(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")),K(TEXT("/Engine/BasicShapes/Cone.Cone"));
    Cube=C.Object;Sphere=S.Object;Cylinder=Y.Object;Cone=K.Object;
}
void AEWFishing::BeginPlay()
{
    Super::BeginPlay();Random.GenerateNewSeed();
    if(auto* G=GetGameInstance<UEWGameInstance>())if(G->SocialSession)Store=&G->SocialSession->LocalSaveStore();
    bReady=Store && Store->Integrity();
    if(bReady){Collection=Store->FishRecords();bPatient=Store->Get(TEXT("fish:patient"),TEXT("1"))==TEXT("1");StoreDisplay=Store->Get(TEXT("fish:display"));}
    Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_LifeSurface.M_LifeSurface"));
    if(!Surface)Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    BuildScene();UpdateDisplay();
}
UStaticMeshComponent* AEWFishing::Shape(USceneComponent* Parent,UStaticMesh* Mesh,FVector Position,FVector Size,FLinearColor Colour)
{
    auto* M=NewObject<UStaticMeshComponent>(this);M->SetupAttachment(Parent);M->SetMobility(EComponentMobility::Movable);
    M->SetStaticMesh(Mesh);M->SetRelativeLocation(Position);M->SetRelativeScale3D(Size/100.);
    M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetCanEverAffectNavigation(false);
    if(Surface){auto* Mat=UMaterialInstanceDynamic::Create(Surface,this);Mat->SetVectorParameterValue(TEXT("Tint"),Colour);Materials.Add(Mat);M->SetMaterial(0,Mat);}
    M->RegisterComponent();return M;
}
void AEWFishing::Beam(UStaticMeshComponent* M,FVector A,FVector B,double Radius)
{
    const auto D=B-A;M->SetRelativeLocation((A+B)*.5);M->SetRelativeRotation(FRotationMatrix::MakeFromZ(D).ToQuat());
    M->SetRelativeScale3D(FVector(Radius*2/100.,Radius*2/100.,D.Size()/100.));
}
void AEWFishing::BuildFish(USceneComponent* Parent,const EWFishing::FSpecies& Fish,double Length)
{
    const double L=Length,H=L*.34*Fish.Slenderness;
    DisplayBody=Shape(Parent,Sphere,{0,0,0},{L*.76,L*.23,H},Fish.Colour);
    auto* Tail=Shape(Parent,Cone,{-L*.43,0,0},{H*1.15,L*.07,L*.28},Fish.Colour*.72f);
    Tail->SetRelativeRotation(FRotator(-90,0,0));
    auto* Dorsal=Shape(Parent,Cone,{-L*.06,0,H*.40},{L*.34,L*.055,H*.6},Fish.Colour*.85f);
    Dorsal->SetRelativeRotation(FRotator(0,0,0));
    for(double Side:{-1.,1.})
    {
        Shape(Parent,Sphere,{L*.23,Side*L*.098,H*.10},{L*.061,L*.022,L*.061},FLinearColor(.96,.91,.70,1));
        Shape(Parent,Sphere,{L*.24,Side*L*.108,H*.10},{L*.031,L*.016,L*.035},FLinearColor(.009,.02,.028,1));
        auto* Fin=Shape(Parent,Cone,{0,Side*L*.105,-H*.1},{L*.16,L*.055,H*.5},Fish.Colour*.8f);
        Fin->SetRelativeRotation(FRotator(20,0,Side*58));
    }
}
void AEWFishing::BuildScene()
{
    auto Node=[&](USceneComponent* Parent,FVector Position)
    {auto* N=NewObject<USceneComponent>(this);N->SetupAttachment(Parent);N->SetRelativeLocation(Position);N->RegisterComponent();return N;};
    auto CameraObstacle=[&](USceneComponent* Parent,FVector Position,FVector Extent)
    {
        auto* Box=NewObject<UBoxComponent>(this);Box->SetupAttachment(Parent);Box->SetRelativeLocation(Position);Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionObjectType(ECC_WorldDynamic);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        Box->SetCanEverAffectNavigation(false);Box->RegisterComponent();
    };
    const FLinearColor Teal(.035,.20,.22,1),Brass(.68,.42,.16,1),Ivory(.89,.88,.73,1);
    for(int32 I=0;I<EWFishing::Spots().Num();++I)
    {
        const auto& S=EWFishing::Spots()[I];const FRotator Rotation(0,S.Yaw,0);
        auto* Site=Node(Root,S.Stand-FVector(0,0,88));Site->SetRelativeRotation(Rotation);SiteRoots.Add(Site);
        // Slim fixtures beside the angler; existing walking surfaces and railings stay passable.
        Shape(Site,Cylinder,{0,-82,57},{8,8,114},Teal);
        Shape(Site,Cylinder,{0,-82,6},{38,38,12},Brass);
        Shape(Site,Cube,{0,-82,124},{12,102,54},Teal);
        CameraObstacle(Site,{0,-82,124},{8,53,29});
        auto* Sign=NewObject<UWidgetComponent>(this);Sign->SetupAttachment(Site);Sign->SetRelativeLocation({-7,-82,124});
        Sign->SetRelativeRotation(FRotator(0,180,0));Sign->SetWidgetSpace(EWidgetSpace::World);Sign->SetDrawSize({720,320});
        Sign->SetTwoSided(true);Sign->SetRelativeScale3D(FVector(.15));Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Sign->SetSlateWidget(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Teal).Padding(15)
            [SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(S.Name)).Font(FSlateFontInfo(FPaths::ProjectContentDir()/TEXT("Fonts/DroidSansFallback.ttf"),40)).ColorAndOpacity(Ivory)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,20)[SNew(STextBlock).Text(FText::FromString(TEXT("FISHING  /  釣り場"))).Font(FSlateFontInfo(FPaths::ProjectContentDir()/TEXT("Fonts/DroidSansFallback.ttf"),29)).ColorAndOpacity(Ivory)]
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("水を眺めて、ひと休み。"))).Font(FSlateFontInfo(FPaths::ProjectContentDir()/TEXT("Fonts/DroidSansFallback.ttf"),26)).ColorAndOpacity(Ivory)]]);
        Sign->RegisterComponent();Signs.Add(Sign);
        auto* Lamp=NewObject<UPointLightComponent>(this);Lamp->SetupAttachment(Site);Lamp->SetRelativeLocation({-15,-82,165});
        Lamp->SetIntensityUnits(ELightUnits::Lumens);Lamp->SetIntensity(3500);Lamp->SetLightColor(FLinearColor(1,.68,.35,1));
        Lamp->SetAttenuationRadius(220);Lamp->SetCastShadows(false);Lamp->SetVolumetricScatteringIntensity(0);Lamp->RegisterComponent();
    }
    Tackle=Node(Root,FVector::ZeroVector);
    Rod=Shape(Tackle,Cylinder,{},FVector(1),Brass);Line=Shape(Tackle,Cylinder,{},FVector(1),Ivory);
    Bobber=Shape(Tackle,Sphere,{},FVector(14,14,20),FLinearColor(.95,.22,.13,1));Tackle->SetVisibility(false,true);
    // A small local collection exhibit on the clock quay. Home aquariums reuse its fish renderer later.
    DisplayRoot=Node(Root,EWFishing::DisplayPosition());DisplayRoot->SetRelativeRotation(FRotator(0,-76,0));
    CameraObstacle(DisplayRoot,{0,0,125},{58,39,34});
    Shape(DisplayRoot,Cylinder,{0,0,44},{46,46,88},Teal);Shape(DisplayRoot,Cube,{0,0,92},{112,72,12},Brass);
    Shape(DisplayRoot,Cube,{0,0,101},{105,66,6},FLinearColor(.06,.24,.29,1));
    Shape(DisplayRoot,Cube,{0,0,153},{112,72,5},Teal);
    for(double X:{-53.,53.})for(double Y:{-33.,33.})Shape(DisplayRoot,Cylinder,{X,Y,126},{3,3,52},Brass);
    Shape(DisplayRoot,Cube,{0,34,126},{107,2,48},FLinearColor(.10,.30,.33,1));
    if(auto* Glass=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_LifeGlass.M_LifeGlass")))
    {
        auto* Front=Shape(DisplayRoot,Cube,{0,-34,126},{106,1,49},FLinearColor::White);Front->SetMaterial(0,Glass);
        for(double X:{-54.,54.})Shape(DisplayRoot,Cube,{X,0,126},{1,66,49},FLinearColor::White)->SetMaterial(0,Glass);
    }
    for(int32 I=0;I<7;++I)Shape(DisplayRoot,Sphere,{-42.+I*13.,8,106},{10,7,5},FLinearColor(.50,.59,.51,1));
    for(int32 I=0;I<4;++I)
    {
        auto* Leaf=Shape(DisplayRoot,Sphere,{-38.+I*4,19,115.+I*4},{5,4,24},FLinearColor(.06,.37,.24,1));
        Leaf->SetRelativeRotation(FRotator(-20+I*12,0,0));
    }
    DisplayFishRoot=Node(DisplayRoot,{0,0,127});
}
void AEWFishing::UpdateDisplay()
{
    if(!DisplayFishRoot)return;
    TArray<USceneComponent*> Old;DisplayFishRoot->GetChildrenComponents(true,Old);
    for(auto* C:Old)if(auto* Mesh=Cast<UStaticMeshComponent>(C))
        {for(int32 I=0;I<Mesh->GetNumMaterials();++I)Materials.Remove(Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(I)));C->DestroyComponent();}
    DisplayBody=nullptr;
    const auto* Fish=EWFishing::Find(StoreDisplay);
    if(Fish && Collection.ContainsByPredicate([&](const EWFishing::FRecord& R){return R.Species==Fish->Id;}))BuildFish(DisplayFishRoot,*Fish,48);
}
AEWCharacter* AEWFishing::Player() const{return Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));}
double AEWFishing::GameHour() const
{const auto* G=GetGameInstance<UEWGameInstance>();return G && G->DayCycle?G->DayCycle->Evidence()->GetNumberField(TEXT("game_hour")):10.;}
int32 AEWFishing::Nearby() const
{
    const auto* G=GetGameInstance<UEWGameInstance>();const auto* P=Player();
    if(!G || !P || !G->Manager || G->Manager->IsTravelling() || !G->SessionStarted() || P->IsSeated() || P->IsLiftRiding() ||
        G->Manager->Descriptor().Seed!=EW::WorldDescriptor::ReferenceWorld().Seed || !P->GetCharacterMovement()->IsMovingOnGround())return -1;
    for(int32 I=0;I<EWFishing::Spots().Num();++I)
    {
        const auto& S=EWFishing::Spots()[I];const FVector At=G->Manager->ToRender(S.Coord,S.Stand),D=P->GetActorLocation()-At;
        if(D.Size2D()>220 || FMath::Abs(D.Z)>65)continue;
        const FVector Aim=G->Manager->ToRender(S.Coord,S.Float);
        if(FVector::DotProduct((Aim-P->Camera->GetComponentLocation()).GetSafeNormal(),P->Camera->GetForwardVector())>.25)return I;
    }
    return -1;
}
bool AEWFishing::Interact()
{
    auto* G=GetGameInstance<UEWGameInstance>();if(!G)return false;
    if(FishingCast.Active()){FishingCast.Press(FPlatformTime::Seconds());return true;}
    if(FishingCast.Phase()==EWFishing::EPhase::Landed){G->SetMenu(EEWMenu::Catch);return true;}
    const int32 Site=Nearby();if(Site<0)return false;
    if(G->SocialSession && G->SocialSession->Active()){G->Notify(TEXT("釣りは一人での散策で遊べます。街での釣りは接続確認後に対応します。"));return true;}
    if(!bReady){G->Notify(SaveError());return true;}
    if(FishingCast.Begin(Site,GameHour(),FPlatformTime::Seconds(),bPatient,Random))
    {ActiveSite=Site;bSaved=bNewSpecies=bNewRecord=false;G->Notify(TEXT("浮きが沈んだら、もう一度 ")+G->InputHint(TEXT("E"),TEXT("X"),TEXT("□"))+TEXT("。連打は不要です。"),5);}
    return true;
}
FString AEWFishing::Hint() const
{
    const auto* G=GetGameInstance<UEWGameInstance>();if(!G)return {};
    const FString E=G->InputHint(TEXT("E"),TEXT("X"),TEXT("□"));
    switch(FishingCast.Phase())
    {
    case EWFishing::EPhase::Waiting:return TEXT("水面を眺めながら、魚を待つ…　歩くと中断");
    case EWFishing::EPhase::Bite:return TEXT("浮きが沈んだ！　")+E+TEXT(" を一度押して引き上げる");
    case EWFishing::EPhase::Reeling:return TEXT("魚をゆっくり引き寄せています…");
    case EWFishing::EPhase::Landed:return TEXT("釣果を見る　")+E;
    default:break;
    }
    const int32 I=Nearby();return I<0?FString():EWFishing::Spots()[I].Name+TEXT("　釣りをする　")+E;
}
void AEWFishing::Cancel()
{
    // Preserve a receipt if disk write failed; menus and travel cannot silently discard it.
    if(FishingCast.Phase()==EWFishing::EPhase::Landed && !bSaved)return;
    FishingCast.Cancel();ActiveSite=-1;if(Tackle)Tackle->SetVisibility(false,true);
}
void AEWFishing::SaveLanded()
{
    if(FishingCast.Phase()!=EWFishing::EPhase::Landed || bSaved)return;
    const auto& Fish=FishingCast.Result();const auto* Previous=Collection.FindByPredicate([&](const EWFishing::FRecord& R){return R.Species==Fish.Species;});
    bNewSpecies=!Previous;bNewRecord=!Previous || Fish.Length>Previous->Largest;
    bool Added=false;bSaved=Store->AddCatch(Fish,Added);
    if(bSaved)Collection=Store->FishRecords();
    if(auto* G=GetGameInstance<UEWGameInstance>())G->SetMenu(EEWMenu::Catch);
}
void AEWFishing::RetrySave(){SaveLanded();}
bool AEWFishing::FlushPending(){SaveLanded();return FishingCast.Phase()!=EWFishing::EPhase::Landed || bSaved;}
void AEWFishing::CastAgain(){if(!bSaved)return;Cancel();if(auto* G=GetGameInstance<UEWGameInstance>())G->SetMenu(EEWMenu::None);Interact();}
void AEWFishing::OpenJournal(){if(auto* G=GetGameInstance<UEWGameInstance>()){if(FishingCast.Active())Cancel();G->SetMenu(EEWMenu::FishJournal);}}
void AEWFishing::TogglePatient()
{
    if(!bReady)return;
    if(Store->Put(TEXT("fish:patient"),bPatient?TEXT("0"):TEXT("1")))bPatient=!bPatient;
    if(auto* G=GetGameInstance<UEWGameInstance>()){if(!Store->Error().IsEmpty())G->Notify(Store->Error());G->RefreshUI();}
}
void AEWFishing::DisplayFish(const FString& Id)
{
    if(!bReady)return;
    if(Store->SetDisplayFish(Id)){StoreDisplay=Id;UpdateDisplay();}
    if(auto* G=GetGameInstance<UEWGameInstance>()){if(!Store->Error().IsEmpty())G->Notify(Store->Error());G->RefreshUI();}
}
void AEWFishing::VisitSpot(int32 Site)
{
    if(!EWFishing::Spots().IsValidIndex(Site))return;auto* G=GetGameInstance<UEWGameInstance>();if(!G || !G->Manager || G->Manager->IsTravelling())return;
    if(FishingCast.Phase()==EWFishing::EPhase::Landed && !bSaved){G->SetMenu(EEWMenu::Catch);return;}
    Cancel();const auto& S=EWFishing::Spots()[Site];EW::PlaceBookmark P;
    P.WorldCode=EW::WorldDescriptor::ReferenceWorld().Code();P.Coord=S.Coord;P.LocalPosition=S.Stand;P.Yaw=S.Yaw;P.Name=S.Name;P.Id=TEXT("fishing/")+S.Id;
    G->Visit(P);if(G->Manager && G->Manager->IsTravelling())ArrivalSite=Site;
}
void AEWFishing::Tick(float Delta)
{
    Super::Tick(Delta);auto* G=GetGameInstance<UEWGameInstance>();auto* P=Player();
    if(!G || !P || !G->Manager)return;auto* M=G->Manager.Get();
    const auto Origin=M->OriginCoord();const bool Local=FMath::Abs(Origin.X)<5 && FMath::Abs(Origin.Y)<5;
    const bool Visible=Local && M->Descriptor().Seed==EW::WorldDescriptor::ReferenceWorld().Seed && !M->IsTravelling();
    SetActorHiddenInGame(!Visible);SetActorEnableCollision(Visible);if(!Visible){Cancel();return;}
    SetActorLocation(M->ToRender({0,0},FVector::ZeroVector));
    if(ArrivalSite>=0)
    {
        const auto& S=EWFishing::Spots()[ArrivalSite];
        if(P->Controller)P->Controller->SetControlRotation((M->ToRender(S.Coord,S.Float+FVector(0,0,100))-P->Camera->GetComponentLocation()).Rotation());
        ArrivalSite=-1;
    }
    for(int32 I=0;I<SiteRoots.Num();++I)SiteRoots[I]->SetVisibility(M->ReadyAt(EWFishing::Spots()[I].Coord),true);
    const double Now=FPlatformTime::Seconds();
    if(DisplayFishRoot){DisplayFishRoot->SetRelativeLocation({FMath::Sin(Now*.43)*20.,0,126+FMath::Sin(Now*.83)*3});DisplayFishRoot->SetRelativeRotation(FRotator(0,FMath::Cos(Now*.43)*12.,FMath::Sin(Now*1.2)*3));}
    if(FishingCast.Active())
    {
        const auto& S=EWFishing::Spots()[ActiveSite];const FVector At=M->ToRender(S.Coord,S.Stand);
        if(G->Menu()!=EEWMenu::None || G->SocialSession && G->SocialSession->Active() || P->IsStreamingHeld() ||
            !P->GetCharacterMovement()->IsMovingOnGround() || FVector::DistSquared(P->GetActorLocation(),At)>240.*240.){Cancel();return;}
        const auto Before=FishingCast.Phase();FishingCast.Tick(Now);
        if(FishingCast.Phase()==EWFishing::EPhase::Landed){SaveLanded();Tackle->SetVisibility(false,true);return;}
        if(FishingCast.Phase()==EWFishing::EPhase::Escaped){Cancel();G->Notify(TEXT("魚は水へ戻っていきました。また竿を出せます。"));return;}
        if(Before!=FishingCast.Phase() && FishingCast.Phase()==EWFishing::EPhase::Bite)G->Notify(TEXT("浮きが沈みました！　一度だけ ")+G->InputHint(TEXT("E"),TEXT("X"),TEXT("□"))+TEXT(" を押してください。"),6);
        Tackle->SetVisibility(true,true);
        const FVector Eye=M->ToLocal({0,0},P->Camera->GetComponentLocation());
        const FVector Aim=(S.Float-Eye).GetSafeNormal();const FVector Side=FVector::CrossProduct(Aim,FVector::UpVector).GetSafeNormal();
        const FVector Handle=Eye+Aim*45+Side*28-FVector(0,0,42),Tip=Handle+Aim*240+FVector(0,0,105+FMath::Sin(Now*2)*2);
        FVector Float=S.Float+FVector(0,0,FMath::Sin(Now*2.7)*2);
        if(FishingCast.Phase()==EWFishing::EPhase::Bite)Float.Z-=FMath::Abs(FMath::Sin(Now*6))*18;
        if(FishingCast.Phase()==EWFishing::EPhase::Reeling)Float=FMath::Lerp(Float,Handle,FishingCast.Progress(Now));
        Beam(Rod,Handle,Tip,.75);Beam(Line,Tip,Float,.12);Bobber->SetRelativeLocation(Float);
    }
}
void AEWFishing::EndPlay(const EEndPlayReason::Type Reason){Store=nullptr;Super::EndPlay(Reason);}
FString AEWFishing::FishingEvidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("phase"),LexToString(int32(FishingCast.Phase())));O->SetNumberField(TEXT("site"),ActiveSite);
    O->SetNumberField(TEXT("nearby"),Nearby());O->SetBoolField(TEXT("saved"),bSaved);O->SetBoolField(TEXT("store_integrity"),Store && Store->Integrity());
    O->SetNumberField(TEXT("species_collected"),Collection.Num());O->SetNumberField(TEXT("catalogue_size"),EWFishing::Species().Num());
    O->SetStringField(TEXT("display"),StoreDisplay);O->SetNumberField(TEXT("hour"),GameHour());O->SetBoolField(TEXT("patient"),bPatient);
    O->SetBoolField(TEXT("surface_loaded"),Surface && Surface->GetName()==TEXT("M_LifeSurface"));O->SetBoolField(TEXT("visible"),!IsHidden());
    TArray<TSharedPtr<FJsonValue>> Records;for(const auto& R:Collection){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("species"),R.Species);J->SetNumberField(TEXT("count"),double(R.Count));J->SetNumberField(TEXT("largest"),R.Largest);Records.Add(MakeShared<FJsonValueObject>(J));}O->SetArrayField(TEXT("records"),Records);
    if(auto* P=Player())
    {
        O->SetStringField(TEXT("player"),P->GetActorLocation().ToString());O->SetBoolField(TEXT("grounded"),P->GetCharacterMovement()->IsMovingOnGround());
        O->SetStringField(TEXT("eye"),P->Camera->GetComponentLocation().ToString());O->SetStringField(TEXT("look"),P->Camera->GetComponentRotation().ToString());
#if WITH_EDITOR
        if(auto* G=GetGameInstance<UEWGameInstance>())if(G->Manager)
        {
            TArray<TSharedPtr<FJsonValue>> Views;
            for(const auto& Site:EWFishing::Spots())
            {
                auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("site"),Site.Id);
                FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(FishingViewAudit),false,P);Query.AddIgnoredActor(this);
                const auto Target=G->Manager->ToRender(Site.Coord,Site.Float);
                const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,P->Camera->GetComponentLocation(),Target,ECC_Visibility,Query);
                V->SetBoolField(TEXT("blocked"),Blocked);V->SetStringField(TEXT("target"),Target.ToString());
                if(Blocked){V->SetStringField(TEXT("hit"),Hit.ImpactPoint.ToString());V->SetStringField(TEXT("component"),GetNameSafe(Hit.GetComponent()));}
                Views.Add(MakeShared<FJsonValueObject>(V));
            }
            O->SetArrayField(TEXT("water_views"),Views);
        }
#endif
    }
    return EWSocial::Encode(O);
}
bool AEWFishing::PreviewFishing(int32 Site)
{
#if WITH_EDITOR
    if(GetWorld()->WorldType!=EWorldType::PIE || !EWFishing::Spots().IsValidIndex(Site))return false;
    VisitSpot(Site);return true;
#else
    return false;
#endif
}
bool AEWFishing::FishingAction(const FString& Action)
{
#if WITH_EDITOR
    if(GetWorld()->WorldType!=EWorldType::PIE)return false;
    if(Action==TEXT("interact"))return Interact();
    if(Action==TEXT("journal")){OpenJournal();return true;}
    if(Action==TEXT("display-last") && bSaved){DisplayFish(FishingCast.Result().Species);return true;}
    if(Action==TEXT("resume")){if(auto* G=GetGameInstance<UEWGameInstance>()){G->SetMenu(EEWMenu::None);return true;}}
    if(Action==TEXT("cancel")){Cancel();return true;}
#endif
    return false;
}
