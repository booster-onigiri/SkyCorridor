#include "EWCinematicCapture95.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWDayCycle.h"
#include "EWExplorationPlan.h"
#include "EWOuterWater.h"
#include "EWSkyTheatrePlan.h"
#include "EWSkyrail.h"
#include "EWLift.h"
#include "EWAeroYacht.h"
#include "EWPhotoMode.h"
#include "Scalability.h"
#include "UnrealClient.h"
#include "Math/Float16Color.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameEngine.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "FrameGrabber.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

struct FCinematicPayload95 : IFramePayload
{
    int32 Number;
    uint64 EngineFrame;
    FVector Eye;
    FRotator Rotation;
    FTransform TrainFrame;
    double TrainClock;
    int32 TrainLine,TrainCar;
    FTransform LiftFrame,YachtFrame;
    double VehicleClock=0;
    FString LiftId;
    int32 YachtIndex=-1;
    bool bHasTrain=false,bHasLift=false,bHasYacht=false;
    FCinematicPayload95(int32 N,const ACameraActor& Camera,const AEWSkyrail* Train,double Clock,int32 Car,const AEWLift* Lift,const AEWAeroYacht* Yacht,double InVehicleClock)
        :Number(N),EngineFrame(GFrameCounter),Eye(Camera.GetActorLocation()),Rotation(Camera.GetActorRotation()),
         TrainClock(Clock),TrainLine(Train?Train->Line:-1),TrainCar(Car)
    {
        if(Train)bHasTrain=Train->CinematicCarFrame(Car,TrainFrame);
        VehicleClock=InVehicleClock;
        if(Lift){bHasLift=Lift->CinematicCabinFrame(LiftFrame);LiftId=Lift->Spec.Id;}
        if(Yacht){bHasYacht=Yacht->CinematicDeckFrame(YachtFrame);YachtIndex=Yacht->Index;}
    }
};
namespace
{
TArray<TSharedPtr<FJsonValue>> XYZ(const FVector& P)
{return {MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y),MakeShared<FJsonValueNumber>(P.Z)};}
}

AEWCinematicCapture95::AEWCinematicCapture95()
{
    PrimaryActorTick.bCanEverTick=true;
    // UE5.8 LevelTick updates PlayerCameraManager AFTER PostPhysics, BEFORE
    // PostUpdateWork. Moving the camera in PostUpdateWork captures the old POV.
    PrimaryActorTick.TickGroup=TG_PostPhysics;
}
AEWCinematicCapture95::~AEWCinematicCapture95() = default;

void AEWCinematicCapture95::BeginPlay()
{
    Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();
    const TCHAR* Args=FCommandLine::Get();
    FParse::Value(Args,TEXT("EWCaptureShot="),Shot);FParse::Value(Args,TEXT("EWCaptureFrames="),Frames);
    FParse::Value(Args,TEXT("EWCaptureFPS="),FPS);
    bCinematic96=FParse::Param(Args,TEXT("EWCinematic96"));
    int32 HDR=0;FParse::Value(Args,TEXT("EWCaptureHDR="),HDR);bHDRCapture=HDR==1;
    if(HDR<0 || HDR>1 || (bHDRCapture && !bCinematic96)){Finish(TEXT("HDR capture requires EWCinematic96 and EWCaptureHDR=1"));return;}
    FParse::Value(Args,TEXT("EWCaptureWidth="),Width);FParse::Value(Args,TEXT("EWCaptureHeight="),Height);
    FParse::Value(Args,TEXT("EWCaptureWarmup="),Warmup);FParse::Value(Args,TEXT("EWCaptureTimeout="),Timeout);
    FString Data,User;
    if(!FParse::Value(Args,TEXT("EWCaptureDir="),Output) || FPaths::IsRelative(Output))
    {Finish(TEXT("an absolute new EWCaptureDir is required"));return;}
    Output=FPaths::ConvertRelativePathToFull(Output);FPaths::NormalizeDirectoryName(Output);
    if(IFileManager::Get().DirectoryExists(*Output))
    {Output.Reset();Finish(TEXT("capture directory must be new"));return;}
    if(!IFileManager::Get().MakeDirectory(*Output,true)){Output.Reset();Finish(TEXT("capture directory creation failed"));return;}
    if(!FParse::Param(Args,TEXT("EWCinematic95")) || !FParse::Value(Args,TEXT("EWDataDir="),Data) ||
       !FParse::Value(Args,TEXT("UserDir="),User) || FPaths::IsRelative(Data) ||
       FPaths::IsRelative(User))
    {Finish(TEXT("explicit absolute isolated EWDataDir and UserDir required"));return;}
    if(Shot<0 || Shot>5 || Frames<1 || Frames>(bCinematic96?960:360) || (FPS!=30 && FPS!=60) ||
       (!bCinematic96 && FPS!=30) || Width<640 || Width>3840 || Height<360 || Height>2160 ||
       !FMath::IsFinite(Warmup) || !FMath::IsFinite(Timeout))
    {Finish(TEXT("capture argument outside finite supported bounds"));return;}
    if(FParse::Value(Args,TEXT("EWCaptureCamera="),CameraOverridePath))
    {
        FPaths::NormalizeFilename(CameraOverridePath);
        if(FPaths::IsRelative(CameraOverridePath))
        {Finish(TEXT("camera override must be an absolute JSON file"));return;}
        CameraOverridePath=FPaths::ConvertRelativePathToFull(CameraOverridePath);
        if(!IFileManager::Get().FileExists(*CameraOverridePath))
        {Finish(TEXT("camera override JSON file missing"));return;}
    }
    Warmup=FMath::Clamp(Warmup,12.,30.);Timeout=FMath::Clamp(Timeout,30.,bCinematic96?3600.:900.);
    if(bCinematic96)
    {
        Scalability::FQualityLevels Quality;Quality.SetFromSingleQualityLevel(4);Quality.ResolutionQuality=100;
        Scalability::SetQualityLevels(Quality,true);
        const TPair<const TCHAR*,float> Settings[]={
            {TEXT("r.ScreenPercentage"),100},{TEXT("r.SecondaryScreenPercentage.GameViewport"),100},
            {TEXT("r.AntiAliasingMethod"),4},{TEXT("r.MotionBlurQuality"),0},
            {TEXT("r.LumenScene.SurfaceCache.AtlasSize"),8192},{TEXT("r.Streaming.PoolSize"),4096},
            {TEXT("r.Shadow.Virtual.MaxPhysicalPages"),8192},{TEXT("r.MaxAnisotropy"),16},
            {TEXT("r.Nanite.MaxPixelsPerEdge"),.5f},{TEXT("r.ScreenshotDelegate"),1}};
        for(const auto& S:Settings)
        {
            auto* C=IConsoleManager::Get().FindConsoleVariable(S.Key);
            if(!C){Finish(FString(TEXT("capture quality cvar missing: "))+S.Key);return;}
            C->Set(S.Value,ECVF_SetByCode);
        }
    }
    PreviousFixed=FApp::UseFixedTimeStep();PreviousFixedDelta=FApp::GetFixedDeltaTime();
    FApp::SetFixedDeltaTime(1./FPS);FApp::SetUseFixedTimeStep(true);OwnsFixed=true;
    if(auto* C=IConsoleManager::Get().FindConsoleVariable(TEXT("framegrabber.framelatency")))C->Set(0,ECVF_SetByCode);
    if(GEngine)GEngine->bEnableOnScreenDebugMessages=false;
    UE_LOG(LogTemp,Display,TEXT("Cinematic95 start shot=%d frames=%d directory=%s"),Shot,Frames,*Output);
}

bool AEWCinematicCapture95::ApplyCameraOverride(float& FOV)
{
    if(CameraOverridePath.IsEmpty())return true;
    FString Text;TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Text,*CameraOverridePath) ||
       !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid())
    {Finish(TEXT("camera override is not a valid JSON object"));return false;}
    for(const auto& Entry:Root->Values)
    {
        // UE5.8 may store JSON keys as FSharedString; its supported operator*
        // exposes a null-terminated string, unlike FString's indexing API.
        const FString EntryKey(*Entry.Key);
        if(EntryKey.Len()!=1 || EntryKey[0]<TCHAR('0') || EntryKey[0]>TCHAR('5') || !Entry.Value.IsValid() || Entry.Value->Type!=EJson::Object)
        {Finish(TEXT("camera override keys must be shot indices 0..5 with object values"));return false;}
    }
    const FString Key=FString::FromInt(Shot);
    if(!Root->HasField(Key))return true;
    const auto Config=Root->GetObjectField(Key);
    for(const auto& Entry:Config->Values)
    {
        const FString EntryKey(*Entry.Key);
        if(EntryKey!=TEXT("eye_start") && EntryKey!=TEXT("eye_end") && EntryKey!=TEXT("aim_start") && EntryKey!=TEXT("aim_end") && EntryKey!=TEXT("fov") &&
           EntryKey!=TEXT("chunk") && EntryKey!=TEXT("bookmark") && EntryKey!=TEXT("train_line") && EntryKey!=TEXT("train_clock_start") &&
           EntryKey!=TEXT("train_car") && EntryKey!=TEXT("camera_space") && EntryKey!=TEXT("aim_space") &&
           EntryKey!=TEXT("lift_id") && EntryKey!=TEXT("lift_from") && EntryKey!=TEXT("lift_to") && EntryKey!=TEXT("lift_clock_start") &&
           EntryKey!=TEXT("yacht_index") && EntryKey!=TEXT("yacht_clock_start"))
        {Finish(TEXT("unknown camera override field: ")+EntryKey);return false;}
    }
    auto VectorField=[&](const TCHAR* Field,FVector& Target)
    {
        if(!Config->HasField(Field))return true;
        const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if(!Config->TryGetArrayField(Field,Values) || !Values || Values->Num()!=3)return false;
        double N[3];
        for(int32 I=0;I<3;++I)
            if(!(*Values)[I].IsValid() || (*Values)[I]->Type!=EJson::Number || !(*Values)[I]->TryGetNumber(N[I]) || !FMath::IsFinite(N[I]) || FMath::Abs(N[I])>500000.)return false;
        Target=FVector(N[0],N[1],N[2]);return true;
    };
    if(!VectorField(TEXT("eye_start"),EyeStart) || !VectorField(TEXT("eye_end"),EyeEnd) ||
       !VectorField(TEXT("aim_start"),AimStart) || !VectorField(TEXT("aim_end"),AimEnd))
    {Finish(TEXT("camera vectors require three finite numbers with absolute values <=500000"));return false;}
    if(Config->HasField(TEXT("fov")))
    {
        double Value;const auto Field=Config->TryGetField(TEXT("fov"));
        if(!Field.IsValid() || Field->Type!=EJson::Number || !Field->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value<35. || Value>95.)
        {Finish(TEXT("camera fov must be finite and in 35..95 degrees"));return false;}
        FOV=float(Value);
    }
    if(Config->HasField(TEXT("chunk")))
    {
        const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;double X,Y;
        if(!Config->TryGetArrayField(TEXT("chunk"),Values) || !Values || Values->Num()!=2 ||
           !(*Values)[0].IsValid() || !(*Values)[1].IsValid() || (*Values)[0]->Type!=EJson::Number || (*Values)[1]->Type!=EJson::Number ||
           !(*Values)[0]->TryGetNumber(X) || !(*Values)[1]->TryGetNumber(Y) || !FMath::IsFinite(X) || !FMath::IsFinite(Y) ||
           FMath::Abs(X)>16 || FMath::Abs(Y)>16 || X!=FMath::FloorToDouble(X) || Y!=FMath::FloorToDouble(Y))
        {Finish(TEXT("chunk requires two bounded integer coordinates in -16..16"));return false;}
        ChunkX=int64(X);ChunkY=int64(Y);
        if(!Config->HasField(TEXT("bookmark"))){Finish(TEXT("chunk override requires an explicit streaming bookmark"));return false;}
    }
    if(!VectorField(TEXT("bookmark"),StreamBookmark))
    {Finish(TEXT("bookmark requires three finite chunk-local numbers"));return false;}
    auto NumberField=[&](const TCHAR* Field,double Min,double Max,double& Result,bool Integer=false)
    {
        const auto Value=Config->TryGetField(Field);
        return Value.IsValid() && Value->Type==EJson::Number && Value->TryGetNumber(Result) && FMath::IsFinite(Result) &&
            Result>=Min && Result<=Max && (!Integer || Result==FMath::FloorToDouble(Result));
    };
    if(Config->HasField(TEXT("train_line")))
    {
        double Line,Car=0;
        if(!NumberField(TEXT("train_line"),0,1,Line,true) || !NumberField(TEXT("train_clock_start"),0,600,TrainClockStart) ||
           (Config->HasField(TEXT("train_car")) && !NumberField(TEXT("train_car"),0,1,Car,true)))
        {Finish(TEXT("train capture requires line 0..1, clock 0..600, and optional car 0..1"));return false;}
        TrainLine=int32(Line);TrainCar=int32(Car);
    }
    else if(Config->HasField(TEXT("train_clock_start")) || Config->HasField(TEXT("train_car")))
    {Finish(TEXT("train clock/car requires train_line"));return false;}
    if(Config->HasField(TEXT("lift_id")))
    {
        double From,To;
        if(!bCinematic96 || !Config->TryGetStringField(TEXT("lift_id"),LiftId) || LiftId.IsEmpty() || LiftId.Len()>128 ||
           !NumberField(TEXT("lift_from"),0,127,From,true) || !NumberField(TEXT("lift_to"),0,127,To,true) || From==To ||
           !NumberField(TEXT("lift_clock_start"),0,600,LiftClockStart))
        {Finish(TEXT("lift requires cinematic96, ID, distinct stops 0..127 and clock 0..600"));return false;}
        LiftFrom=int32(From);LiftTo=int32(To);
    }
    else if(Config->HasField(TEXT("lift_from")) || Config->HasField(TEXT("lift_to")) || Config->HasField(TEXT("lift_clock_start")))
    {Finish(TEXT("lift stops/clock require lift_id"));return false;}
    if(Config->HasField(TEXT("yacht_index")))
    {
        double Index;
        if(!bCinematic96 || !NumberField(TEXT("yacht_index"),0,1,Index,true) || !NumberField(TEXT("yacht_clock_start"),0,600,YachtClockStart))
        {Finish(TEXT("yacht requires cinematic96, index 0..1 and clock 0..600"));return false;}
        YachtIndex=int32(Index);
    }
    else if(Config->HasField(TEXT("yacht_clock_start")))
    {Finish(TEXT("yacht clock requires yacht_index"));return false;}
    if(int32(TrainLine>=0)+int32(!LiftId.IsEmpty())+int32(YachtIndex>=0)>1)
    {Finish(TEXT("a shot may control only one vehicle"));return false;}
    FString Space=TEXT("chunk");
    if(Config->HasField(TEXT("camera_space")) && !Config->TryGetStringField(TEXT("camera_space"),Space))
    {Finish(TEXT("camera_space must be a string"));return false;}
    if((Space!=TEXT("chunk") && Space!=TEXT("train") && Space!=TEXT("lift") && Space!=TEXT("yacht")) ||
       (Space==TEXT("train") && TrainLine<0) || (Space==TEXT("lift") && LiftId.IsEmpty()) || (Space==TEXT("yacht") && YachtIndex<0))
    {Finish(TEXT("vehicle-relative camera requires the corresponding selected vehicle"));return false;}
    bTrainCamera=Space==TEXT("train");bLiftCamera=Space==TEXT("lift");bYachtCamera=Space==TEXT("yacht");
    FString AimSpace=TEXT("camera");
    if(Config->HasField(TEXT("aim_space")) && (!bCinematic96 || !Config->TryGetStringField(TEXT("aim_space"),AimSpace)))
    {Finish(TEXT("aim_space requires cinematic96 and a string"));return false;}
    if(AimSpace!=TEXT("camera") && AimSpace!=TEXT("chunk"))
    {Finish(TEXT("aim_space must be camera or chunk"));return false;}
    bChunkAim=AimSpace==TEXT("chunk");
    if((bTrainCamera || bLiftCamera || bYachtCamera) && (!Config->HasField(TEXT("eye_start")) || !Config->HasField(TEXT("eye_end")) ||
       !Config->HasField(TEXT("aim_start")) || !Config->HasField(TEXT("aim_end"))))
    {Finish(TEXT("vehicle-relative camera requires all four explicit eye/aim vectors"));return false;}
    CameraOverrideApplied=true;Source+=TEXT("; explicit JSON camera space: ")+Space;return true;
}

bool AEWCinematicCapture95::PrepareShot()
{
    auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(!G || !M || !P || !M->ReadyAt({0,0}) || M->IsTravelling() || P->IsStreamingHeld())return false;
    if(!G->bScriptedWorldAudit){Finish(TEXT("scripted input isolation absent"));return false;}
    const auto R=M->RecipeAt({0,0});if(!R)return false;
    EW::ChunkCoord C{0,0};FVector Foot;float FOV=67;
    if(Shot==0)
    {
        Name=TEXT("01-clock-plaza");Source=TEXT("EWLightingAudit plaza view / recipe Hub");
        EyeStart=R->Hub+FVector(-150,-1650,164);EyeEnd=R->Hub+FVector(150,-1250,195);
        AimStart=R->Hub+FVector(0,0,800);AimEnd=AimStart;Foot=EyeStart-FVector(0,0,74);
    }
    else if(Shot==1 || Shot==2)
    {
        const auto* Room=EWExplorationPlan::Find(*R,Shot==1?0:2);
        if(!Room){Finish(TEXT("required public interior missing"));return false;}
        const auto T=Room->Frame;Source=TEXT("EWExplorationPlan room frame and EWExplore85Data traversable route");
        if(Shot==1)
        {
            const auto* Table=R->Parts.FindByPredicate([](const auto& V){return V.Mesh==TEXT("Craft95CafeTableSet");});
            if(!Table){Finish(TEXT("craft95 cafe table sample missing"));return false;}
            Name=TEXT("02-water-garden-cafe");Source+=TEXT("; EWCraft95::Apply Craft95CafeTableSet actual recipe transform");
            // Start outside the table's solid envelope, then dolly along the
            // open entry route while turning from the joinery into the cafe.
            EyeStart=T.TransformPosition(FVector(-420,-1730,150));EyeEnd=T.TransformPosition(FVector(-120,-950,175));
            AimStart=Table->Transform.TransformPosition(FVector(0,0,80));AimEnd=T.TransformPosition(FVector(-200,1100,240));
            Foot=T.TransformPosition(FVector(0,-1750,100));
        }
        else
        {
            Name=TEXT("03-sky-library");EyeStart=T.TransformPosition(FVector(-1410,1500,584));EyeEnd=T.TransformPosition(FVector(-1050,1500,584));
            AimStart=T.TransformPosition(FVector(350,-350,380));AimEnd=T.TransformPosition(FVector(200,-700,330));
            Foot=T.TransformPosition(FVector(-1410,1500,520));FOV=75;
        }
    }
    else if(Shot==3)
    {
        C={5,1};const double H=EWOuterWater::Ground(M->Descriptor(),C);
        Name=TEXT("04-glass-water-city");Source=TEXT("EWCascadeAudit outer district 5,1 view 1 and EWOuterWater::Ground");
        EyeStart=FVector(6150,10800,H+1065);EyeEnd=FVector(6750,10600,H+1190);
        AimStart=FVector(15000,3000,H+3800);AimEnd=FVector(14800,3300,H+3650);
        const auto Outer=EW::GenerateChunk(M->Descriptor(),C);
        if(Outer.Places.IsEmpty()){Finish(TEXT("water city dry bookmark missing"));return false;}Foot=Outer.Places[0].LocalPosition;FOV=78;
    }
    else
    {
        FTransform T;
        if(Shot==4)
        {
            const auto* Part=R->Parts.FindByPredicate([](const auto& V){return V.Mesh.ToString().StartsWith(TEXT("UrbanGarden_"));});
            if(!Part){Finish(TEXT("rooftop garden missing"));return false;}T=Part->Transform;
            Name=TEXT("05-rooftop-garden");Source=TEXT("EWLightingAudit rooftop garden recipe frame");
            EyeStart=T.TransformPosition(FVector(-650,1700,164));EyeEnd=T.TransformPosition(FVector(-250,1650,205));
            AimStart=T.TransformPosition(FVector(1500,-900,680));AimEnd=T.TransformPosition(FVector(1750,-1150,660));
            Foot=T.TransformPosition(FVector(-650,1700,100));
        }
        else
        {
            if(!EWSkyTheatrePlan::Frame(*R,T)){Finish(TEXT("sky theatre deck missing"));return false;}
            Name=TEXT("06-sky-corridor-vista");Source=TEXT("EWLightingAudit highest deck and EWSkyTheatrePlan::Frame");
            EyeStart=T.TransformPosition(FVector(-2050,1500,170));EyeEnd=T.TransformPosition(FVector(-1650,1500,260));
            AimStart=T.TransformPosition(FVector(-12000,-16000,-2200));AimEnd=T.TransformPosition(FVector(-10000,-17500,-1800));
            Foot=T.TransformPosition(EWSkyTheatrePlan::Entry());FOV=75;
        }
    }
    // Defaults keep the original landmark bookmark. A film-only override may
    // select another resident chunk/bookmark without changing normal world data.
    ChunkX=C.X;ChunkY=C.Y;StreamBookmark=Foot;
    if(!ApplyCameraOverride(FOV))return false;
    C={ChunkX,ChunkY};Foot=StreamBookmark;
    if(TrainLine>=0)FilmedTrain=TrainLine?G->SkyrailUpper.Get():G->Skyrail.Get();
    if(TrainLine>=0 && !FilmedTrain){Finish(TEXT("requested train actor absent"));return false;}
    if(YachtIndex>=0)
    {
        for(auto Y:G->Yachts)if(Y && Y->Index==YachtIndex){FilmedYacht=Y;break;}
        if(!FilmedYacht){Finish(TEXT("requested yacht actor absent"));return false;}
    }
    auto B=M->CurrentPosition();B.Coord=C;B.LocalPosition=Foot;G->Visit(B);
    // ToRender may change during Visit; convert after streaming settles in phase 1.
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("shot"),Name);J->SetStringField(TEXT("coordinate_source"),Source);
    J->SetStringField(TEXT("chunk"),C.Text());J->SetNumberField(TEXT("fov"),FOV);
    J->SetStringField(TEXT("override_file"),CameraOverridePath);J->SetBoolField(TEXT("override_applied"),CameraOverrideApplied);
    J->SetStringField(TEXT("camera_space"),bTrainCamera?TEXT("train"):bLiftCamera?TEXT("lift"):bYachtCamera?TEXT("yacht"):TEXT("chunk"));
    J->SetStringField(TEXT("aim_space"),bChunkAim?TEXT("chunk"):TEXT("camera"));J->SetArrayField(TEXT("stream_bookmark"),XYZ(Foot));
    J->SetNumberField(TEXT("train_line"),TrainLine);J->SetNumberField(TEXT("train_car"),TrainCar);J->SetNumberField(TEXT("train_clock_start"),TrainClockStart);
    J->SetStringField(TEXT("lift_id"),LiftId);J->SetNumberField(TEXT("lift_from"),LiftFrom);J->SetNumberField(TEXT("lift_to"),LiftTo);J->SetNumberField(TEXT("lift_clock_start"),LiftClockStart);
    J->SetNumberField(TEXT("yacht_index"),YachtIndex);J->SetNumberField(TEXT("yacht_clock_start"),YachtClockStart);
    J->SetArrayField(TEXT("local_eye_start"),XYZ(EyeStart));J->SetArrayField(TEXT("local_eye_end"),XYZ(EyeEnd));
    J->SetArrayField(TEXT("local_aim_start"),XYZ(AimStart));J->SetArrayField(TEXT("local_aim_end"),XYZ(AimEnd));
    FrameEvidence.Add(MakeShared<FJsonValueObject>(J));
    Camera=GetWorld()->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(FOV);
    Camera->GetCameraComponent()->bConstrainAspectRatio=false;
    G->SetMenu(EEWMenu::None);Phase=1;Stage=FPlatformTime::Seconds();return true;
}

void AEWCinematicCapture95::Pose(int32 Frame)
{
    const double T=Frames>1?double(Frame)/(Frames-1):.5;
    const double S=T*T*(3.-2.*T);
    auto* M=GetGameInstance<UEWGameInstance>()->Manager.Get();const EW::ChunkCoord C{ChunkX,ChunkY};
    FTransform TrainFrame;
    if(FilmedTrain)
    {
        TrainClockNow=TrainClockStart+double(Frame)/FPS;
        if(!FilmedTrain->SetCinematicClock(TrainClockNow) || !FilmedTrain->CinematicCarFrame(TrainCar,TrainFrame))
        {Finish(TEXT("selected train cannot provide a ready deterministic car transform"));return;}
    }
    FTransform VehicleFrame=TrainFrame;
    if(FilmedLift)
    {
        VehicleClockNow=LiftClockStart+double(Frame)/FPS;
        if(!FilmedLift->SetCinematicTrip(LiftFrom,LiftTo,VehicleClockNow) || !FilmedLift->CinematicCabinFrame(VehicleFrame))
        {Finish(TEXT("lift cannot provide actual deterministic cabin frame"));return;}
    }
    if(FilmedYacht)
    {
        VehicleClockNow=YachtClockStart+double(Frame)/FPS;
        if(!FilmedYacht->SetCinematicClock(VehicleClockNow) || !FilmedYacht->CinematicDeckFrame(VehicleFrame))
        {Finish(TEXT("yacht cannot provide actual deterministic deck frame"));return;}
    }
    if(FilmedLift || FilmedYacht)
        if(auto* P=UGameplayStatics::GetPlayerPawn(this,0))
            P->SetActorLocation(VehicleFrame.TransformPosition(FilmedLift?FVector(0,0,88):FVector(3500,0,699)),false,nullptr,ETeleportType::TeleportPhysics);
    const bool Relative=bTrainCamera || bLiftCamera || bYachtCamera;
    const FVector LocalEye=FMath::Lerp(EyeStart,EyeEnd,S),LocalAim=FMath::Lerp(AimStart,AimEnd,S);
    const FVector Eye=Relative?VehicleFrame.TransformPosition(LocalEye):M->ToRender(C,LocalEye);
    const FVector Aim=Relative && !bChunkAim?VehicleFrame.TransformPosition(LocalAim):M->ToRender(C,LocalAim);
    Camera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
}

void AEWCinematicCapture95::DrainFrames()
{
    if(!Grabber)return;
    auto Captured=Grabber->GetCapturedFrames();
    for(auto& F:Captured)
    {
        const auto* Payload=F.GetPayload<FCinematicPayload95>();
        if(!Payload || F.BufferSize!=FIntPoint(Width,Height) || F.ColorBuffer.Num()!=Width*Height || Received.Contains(Payload->Number))
        {Finish(TEXT("invalid dimensions, duplicate frame or missing payload"));return;}
        // Warmup is proven by renderer readbacks, not elapsed engine ticks. No
        // warmup image is saved or included in the deliverable's frame count.
        if(Payload->Number<0){++WarmupRendered;continue;}
        const FString File=Output/FString::Printf(TEXT("frame_%05d.png"),Payload->Number);
        for(auto& Pixel:F.ColorBuffer)Pixel.A=255;
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Width,Height,TArrayView64<const FColor>(F.ColorBuffer),PNG);
        if(PNG.IsEmpty() || !FFileHelper::SaveArrayToFile(PNG,*File)){Finish(TEXT("frame PNG write failed"));return;}
        RecordPayload(*Payload);
        for(const auto& Still:TArray<TPair<int32,FString>>{{0,TEXT("still-start.png")},{Frames/2,TEXT("still-native.png")},{Frames-1,TEXT("still-end.png")}})
            if(Payload->Number==Still.Key && IFileManager::Get().Copy(*(Output/Still.Value),*File,false)!=COPY_OK)
            {Finish(TEXT("native still copy failed"));return;}
        if(Written%FPS==0)UE_LOG(LogTemp,Display,TEXT("Cinematic95 %s %d/%d"),*Name,Written,Frames);
    }
}

void AEWCinematicCapture95::RecordPayload(const FCinematicPayload95& Payload)
{
    Received.Add(Payload.Number);++Written;
    auto Evidence=MakeShared<FJsonObject>();Evidence->SetNumberField(TEXT("number"),Payload.Number);
    Evidence->SetNumberField(TEXT("engine_frame"),double(Payload.EngineFrame));
    Evidence->SetNumberField(TEXT("simulation_seconds"),double(Payload.Number)/FPS);
    Evidence->SetArrayField(TEXT("camera_eye_render"),XYZ(Payload.Eye));
    Evidence->SetArrayField(TEXT("camera_pitch_yaw_roll"),XYZ(FVector(Payload.Rotation.Pitch,Payload.Rotation.Yaw,Payload.Rotation.Roll)));
    if(Payload.bHasTrain)
    {
        Evidence->SetNumberField(TEXT("train_clock"),Payload.TrainClock);Evidence->SetNumberField(TEXT("train_line"),Payload.TrainLine);
        Evidence->SetNumberField(TEXT("train_car"),Payload.TrainCar);Evidence->SetArrayField(TEXT("train_car_position_render"),XYZ(Payload.TrainFrame.GetLocation()));
        const FRotator R=Payload.TrainFrame.Rotator();Evidence->SetArrayField(TEXT("train_car_pitch_yaw_roll"),XYZ(FVector(R.Pitch,R.Yaw,R.Roll)));
    }
    if(Payload.bHasLift)
    {
        Evidence->SetStringField(TEXT("lift_id"),Payload.LiftId);Evidence->SetNumberField(TEXT("lift_clock"),Payload.VehicleClock);
        Evidence->SetArrayField(TEXT("lift_cabin_position_render"),XYZ(Payload.LiftFrame.GetLocation()));
        const FRotator R=Payload.LiftFrame.Rotator();Evidence->SetArrayField(TEXT("lift_cabin_pitch_yaw_roll"),XYZ(FVector(R.Pitch,R.Yaw,R.Roll)));
    }
    if(Payload.bHasYacht)
    {
        Evidence->SetNumberField(TEXT("yacht_index"),Payload.YachtIndex);Evidence->SetNumberField(TEXT("yacht_clock"),Payload.VehicleClock);
        Evidence->SetArrayField(TEXT("yacht_position_render"),XYZ(Payload.YachtFrame.GetLocation()));
        const FRotator R=Payload.YachtFrame.Rotator();Evidence->SetArrayField(TEXT("yacht_pitch_yaw_roll"),XYZ(FVector(R.Pitch,R.Yaw,R.Roll)));
    }
    PoseEvidence.Add(MakeShared<FJsonValueObject>(Evidence));
}

void AEWCinematicCapture95::QueueFrame(int32 Frame)
{
    auto Payload=MakeShared<FCinematicPayload95,ESPMode::ThreadSafe>(Frame,*Camera,FilmedTrain.Get(),TrainClockNow,TrainCar,FilmedLift.Get(),FilmedYacht.Get(),VehicleClockNow);
    if(!bHDRCapture){Grabber->CaptureThisFrame(Payload);return;}
    if(PendingHDR.IsValid() || FScreenshotRequest::IsScreenshotRequested())
    {Finish(TEXT("previous HDR request did not complete before the next simulation frame"));return;}
    PendingHDR=Payload;
    FScreenshotRequest::RequestScreenshot(Output/TEXT("hdr-viewport.exr"),false,false,true);
}

void AEWCinematicCapture95::CaptureHDR(int32 W,int32 H,const TArray<FLinearColor>& Pixels)
{
    if(Done || !PendingHDR.IsValid())return;
    const auto Payload=PendingHDR;PendingHDR.Reset();
    const auto* G=GetGameInstance<UEWGameInstance>();
    if(!bHDRCapture || !G || !G->Graphics.IsHDRActive() || W!=Width || H!=Height || Pixels.Num()!=Width*Height ||
       Payload->EngineFrame!=GFrameCounter || (Payload->Number>=0 && Received.Contains(Payload->Number)))
    {Finish(TEXT("HDR readback dimensions, display state, frame timing or unique payload mismatch"));return;}
    if(Payload->Number<0){++WarmupRendered;return;}
    const bool Still=Payload->Number==0 || Payload->Number==Frames/2 || Payload->Number==Frames-1;
    TArray<FFloat16Color> Half;Half.Reserve(Pixels.Num());TArray<FColor> Preview;if(Still)Preview.Reserve(Pixels.Num());
    double MaxNits=0,MeanNits=0,MaxRGB=-DBL_MAX,MinRGB=DBL_MAX;int32 Above80=0;
    for(const auto& C:Pixels)
    {
        if(!FMath::IsFinite(C.R) || !FMath::IsFinite(C.G) || !FMath::IsFinite(C.B) ||
           FMath::Max3(FMath::Abs(C.R),FMath::Abs(C.G),FMath::Abs(C.B))>65504.f)
        {Finish(TEXT("HDR source has nonfinite or out-of-half-range pixels"));return;}
        const double Nits=FMath::Max(0.,80.*(.2126*C.R+.7152*C.G+.0722*C.B));
        MaxNits=FMath::Max(MaxNits,Nits);MeanNits+=Nits;Above80+=Nits>80.;
        MinRGB=FMath::Min(MinRGB,double(FMath::Min3(C.R,C.G,C.B)));MaxRGB=FMath::Max(MaxRGB,double(FMath::Max3(C.R,C.G,C.B)));
        Half.Add(FFloat16Color(FLinearColor(C.R,C.G,C.B,1)));if(Still)Preview.Add(EWPhoto::ToSDR(C));
    }
    const FString File=Output/FString::Printf(TEXT("frame_%05d.exr"),Payload->Number);
    // UGameViewportClient converts the actual HDR backbuffer to linear scRGB
    // before this delegate. Half EXR with quality 0 uses lossless ZIP; 1=80 nits.
    if(!FImageUtils::SaveImageByExtension(*File,FImageView(Half.GetData(),W,H),0))
    {Finish(TEXT("HDR half-float ZIP EXR write failed"));return;}
    RecordPayload(*Payload);
    auto Evidence=PoseEvidence.Last()->AsObject();Evidence->SetNumberField(TEXT("hdr_luminance_max_nits"),MaxNits);
    Evidence->SetNumberField(TEXT("hdr_luminance_mean_nits"),MeanNits/Pixels.Num());Evidence->SetNumberField(TEXT("hdr_pixels_above_80_nits"),Above80);
    Evidence->SetNumberField(TEXT("hdr_rgb_min"),MinRGB);Evidence->SetNumberField(TEXT("hdr_rgb_max"),MaxRGB);
    Evidence->SetNumberField(TEXT("readback_engine_frame"),double(GFrameCounter));
    if(Still)
    {
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(W,H,TArrayView64<const FColor>(Preview),PNG);
        for(const auto& Item:TArray<TPair<int32,FString>>{{0,TEXT("still-start.png")},{Frames/2,TEXT("still-native.png")},{Frames-1,TEXT("still-end.png")}})
            if(Payload->Number==Item.Key && (PNG.IsEmpty() || !FFileHelper::SaveArrayToFile(PNG,*(Output/Item.Value))))
            {Finish(TEXT("HDR QA preview write failed"));return;}
    }
    if(Written%FPS==0)UE_LOG(LogTemp,Display,TEXT("Cinematic96 HDR %s %d/%d"),*Name,Written,Frames);
}

void AEWCinematicCapture95::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(Done)return;
    const double Now=FPlatformTime::Seconds();
    if(Now-Started>Timeout){Finish(TEXT("finite capture timeout"));return;}
    if(Phase==0){PrepareShot();return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !M || !P || !PC){Finish(TEXT("capture world lost"));return;}
    const EW::ChunkCoord C{ChunkX,ChunkY};
    if(Phase==1)
    {
        if(M->IsTravelling() || P->IsStreamingHeld() || !M->ReadyAt(C))return;
        // Visit rebuilds chunk actors, so resolve the real cabin only after
        // destination streaming has settled; never retain the retired lift.
        if(!LiftId.IsEmpty() && !IsValid(FilmedLift.Get()))
        {
            for(TActorIterator<AEWLift> It(GetWorld());It;++It)if(It->Spec.Id==LiftId){FilmedLift=*It;break;}
            if(!FilmedLift){Finish(TEXT("requested lift actor absent after destination streaming"));return;}
        }
        P->GetCharacterMovement()->StopMovementImmediately();P->GetCharacterMovement()->DisableMovement();P->SetActorHiddenInGame(true);
        PC->SetViewTarget(Camera);PC->bShowMouseCursor=false;Pose(0);if(Done)return;Phase=2;Stage=Now;return;
    }
    DrainFrames();if(Done)return;
    if(Phase==2)
    {
        Pose(0);if(Done)return;
        if(!Grabber && !HDRHandle.IsValid())
        {
            auto* GE=Cast<UGameEngine>(GEngine);
            if(!GE || !GE->SceneViewport.IsValid()){Finish(TEXT("standalone game scene viewport required; PIE is not supported"));return;}
            if(GE->SceneViewport->GetSizeXY()!=FIntPoint(Width,Height)){Finish(TEXT("actual viewport differs from requested native capture dimensions"));return;}
            if(G->Graphics.IsHDRActive()!=bHDRCapture || G->Graphics.SuperResolution>(bCinematic96?0:1) || G->Graphics.FrameGeneration!=0)
            {Finish(TEXT("capture requires matching actual HDR state, native rendering and frame generation off"));return;}
            if(bHDRCapture)HDRHandle=UGameViewportClient::OnHDRScreenshotCaptured().AddUObject(this,&AEWCinematicCapture95::CaptureHDR);
            else
            {
                Grabber=MakeUnique<FFrameGrabber>(GE->SceneViewport.ToSharedRef(),FIntPoint(Width,Height),PF_B8G8R8A8,3);
                Grabber->StartCapturingFrames();
            }
            UE_LOG(LogTemp,Display,TEXT("Cinematic95 warming camera %s: >=%.1fs and >=120 actual viewport frames"),*Name,Warmup);
        }
        if(Now-Stage<Warmup || WarmupRendered<120)
        {
            QueueFrame(-1);if(Done)return;++WarmupRequested;return;
        }
        // Drain the last warmup request before numbering the first final frame.
        if((Grabber && Grabber->HasOutstandingFrames()) || PendingHDR.IsValid())return;
        UE_LOG(LogTemp,Display,TEXT("Cinematic95 warmup complete: %.2fs, %d viewport frames"),Now-Stage,WarmupRendered);
        Phase=3;
    }
    if(Phase==3 && Requested<Frames)
    {
        if(FMath::Abs(DeltaSeconds-1.f/FPS)>.001f){Finish(TEXT("simulation timestep differs from requested capture FPS"));return;}
        Pose(Requested);if(Done)return;
        QueueFrame(Requested);if(Done)return;++Requested;
        if(Requested==Frames){Phase=4;Stage=Now;}
    }
    if(Phase==4 && Written==Frames){Finish();return;}
    if(Phase==4 && Now-Stage>60){Finish(TEXT("frame readback did not drain"));return;}
}

void AEWCinematicCapture95::Finish(const FString& Error)
{
    if(Done)return;Done=true;
    if(FilmedTrain)FilmedTrain->ReleaseCinematicClock();
    if(FilmedLift)FilmedLift->ReleaseCinematicTrip();
    if(FilmedYacht)FilmedYacht->ReleaseCinematicClock();
    if(HDRHandle.IsValid()){UGameViewportClient::OnHDRScreenshotCaptured().Remove(HDRHandle);HDRHandle.Reset();}
    if(PendingHDR.IsValid()){FScreenshotRequest::Reset();PendingHDR.Reset();}
    if(Grabber){Grabber->StopCapturingFrames();Grabber->Shutdown();Grabber.Reset();}
    if(OwnsFixed){FApp::SetUseFixedTimeStep(PreviousFixed);FApp::SetFixedDeltaTime(PreviousFixedDelta);OwnsFixed=false;}
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty() && Written==Frames);O->SetStringField(TEXT("failure"),Error);
    O->SetStringField(TEXT("shot"),Name);O->SetNumberField(TEXT("requested_frames"),Requested);O->SetNumberField(TEXT("written_frames"),Written);
    O->SetStringField(TEXT("camera_override_file"),CameraOverridePath);O->SetBoolField(TEXT("camera_override_applied"),CameraOverrideApplied);
    O->SetNumberField(TEXT("fps"),FPS);O->SetNumberField(TEXT("width"),Width);O->SetNumberField(TEXT("height"),Height);
    O->SetNumberField(TEXT("duration_seconds"),double(Written)/FPS);O->SetNumberField(TEXT("wall_seconds"),FPlatformTime::Seconds()-Started);
    O->SetNumberField(TEXT("warmup_wall_seconds"),Warmup);O->SetArrayField(TEXT("camera"),FrameEvidence);
    O->SetNumberField(TEXT("warmup_requested_frames"),WarmupRequested);O->SetNumberField(TEXT("warmup_rendered_frames"),WarmupRendered);
    O->SetArrayField(TEXT("frames"),PoseEvidence);O->SetStringField(TEXT("camera_timing"),TEXT("TG_PostPhysics pose before UE5.8 PlayerCameraManager update; payload queued for same rendered frame"));
    O->SetStringField(TEXT("capture_method"),FString::Printf(TEXT("UE actual game viewport %s, one rendered frame per fixed 1/%d-second simulation tick; native dimensions; no desktop capture or interpolated video"),bHDRCapture?TEXT("HDR float screenshot delegate"):TEXT("FFrameGrabber PNG"),FPS));
    O->SetBoolField(TEXT("hdr_capture"),bHDRCapture);O->SetStringField(TEXT("frame_format"),bHDRCapture?TEXT("exr_half_zip"):TEXT("png_bgra8"));
    O->SetStringField(TEXT("source_color_space"),bHDRCapture?TEXT("linear_scRGB_Rec709"):TEXT("sRGB_Rec709"));
    if(bHDRCapture)
    {
        O->SetNumberField(TEXT("source_linear_one_nits"),80);O->SetStringField(TEXT("hdr_capture_origin"),TEXT("UE5.8 UGameViewportClient GetViewportScreenShotHDR then ConvertPixelDataToSCRGB; actual HDR DXGI output required"));
        O->SetStringField(TEXT("preview_scope"),TEXT("still-*.png are tone-mapped SDR QA previews only; numbered half EXRs preserve HDR source"));
    }
    auto Quality=MakeShared<FJsonObject>();Quality->SetBoolField(TEXT("cinematic96"),bCinematic96);
    for(const TCHAR* Key:{TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),
        TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("sg.ShadingQuality"),
        TEXT("r.ScreenPercentage"),TEXT("r.SecondaryScreenPercentage.GameViewport"),TEXT("r.AntiAliasingMethod"),TEXT("r.MotionBlurQuality"),
        TEXT("r.LumenScene.SurfaceCache.AtlasSize"),TEXT("r.Streaming.PoolSize"),TEXT("r.Shadow.Virtual.MaxPhysicalPages"),TEXT("r.MaxAnisotropy"),TEXT("r.Nanite.MaxPixelsPerEdge")})
        if(auto* C=IConsoleManager::Get().FindConsoleVariable(Key))Quality->SetNumberField(Key,C->GetFloat());
    O->SetObjectField(TEXT("capture_quality"),Quality);
    if(auto* G=GetGameInstance<UEWGameInstance>()){O->SetObjectField(TEXT("graphics"),G->Graphics.Evidence());if(G->DayCycle)O->SetObjectField(TEXT("lighting"),G->DayCycle->Evidence());}
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));
    if(!Output.IsEmpty())FFileHelper::SaveStringToFile(Text,*(Output/TEXT("capture.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp,Display,TEXT("Cinematic95 complete success=%d frames=%d error=%s"),Error.IsEmpty() && Written==Frames,Written,*Error);
    FPlatformMisc::RequestExit(false);
}

void AEWCinematicCapture95::EndPlay(const EEndPlayReason::Type Reason)
{
    if(!Done)Finish(TEXT("capture interrupted before completion"));
    Super::EndPlay(Reason);
}
