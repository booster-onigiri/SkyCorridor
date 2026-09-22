#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Containers/Ticker.h"
#include "EWWorld.h"
#include "EWSaveStore.h"
#include "EWGraphics.h"
#include "EWHudLayer.h"
#include "EWGameInstance.generated.h"

class FEWSaveStore;
class AEWChunkManager;
class AEWLift;
class AEWSkyrail;
class AEWMediaScreen;
class AEWAirship;
class AEWAeroYacht;
class AEWAeroPort;
class AEWCharacter;
class AEWTerminal;
class AEWMusic;
class AEWWaterView;
class AEWDayCycle;
class AEWCinemaSession;
class AEWSocialSession;
class AEWFishing;
class AEWPhotoMode;
class AEWConceptRuntime;
enum class EEWMenu : uint8 { None, Main, Journal, Settings, Worlds, SaveFailure, Lift, Monitor, Online, City, FishJournal, Catch, Photo, Workshop, Chess, SkyResidences, Explore, Terminal };

UCLASS()
class ENDLESSWORLD_API UEWGameInstance : public UGameInstance
{
    GENERATED_BODY()
public:
    virtual void Init() override;
    virtual void Shutdown() override;
    void AttachWorld();
    void TravelFinished();
    void NewWorld();
    void ReferenceWorld();
    void ReturnToPlaza();
    UFUNCTION(BlueprintCallable) void VisitCinema();
    UFUNCTION(BlueprintCallable) void VisitSkyTheatre();
    UFUNCTION(BlueprintCallable) void VisitSkyport();
    AEWAeroYacht* YachtFor(const AEWCharacter* Player) const;
    UFUNCTION(BlueprintCallable) void VisitOuterWater();
    UFUNCTION(BlueprintCallable) bool PreviewOuterWater(int32 View,double Hour);
    UFUNCTION(BlueprintCallable) void VisitPublicPlace(int32 Index);
    UFUNCTION(BlueprintCallable) bool PreviewPublicPlace(int32 Index,double Hour,int32 View);
    UFUNCTION(BlueprintCallable) void VisitSkyResidence(int32 Index);
    UFUNCTION(BlueprintCallable) bool PreviewSkyResidence(int32 Index,double Hour,int32 View);
    UPROPERTY() TObjectPtr<class ACameraActor> HotelPreviewCamera;
    UFUNCTION(BlueprintCallable) void VisitSkyrail();
    void ContinueWorld();
    void StartFromCode(const FString& Code);
    void Visit(const EW::PlaceBookmark& Place);
    void RecordNearest();
    void Interact();
    void SelectLiftFloor(int32 Stop);
    void Favourite(const EW::PlaceBookmark& Place);
    void CopyWorld();
    void CopyPlace(const EW::PlaceBookmark& Place);
    void ExportRecords();
    void OpenSaveDirectory();
    void SetMenu(EEWMenu Menu);
    void ToggleMenu();
    void ToggleJournal();
    void RefreshUI();
    void UpdatePresentationState();
    void Quit();
    void QuitWithoutSaving();
    void Capture();
    void SetQuality(bool High);
    void SetNativeResolution();
    void MoveToPrimaryDisplay();
    void SetSoftStyle(bool Value);
    void NoteInput(bool Gamepad, bool Sony = false);
    void SetPadLookSpeed(float Value);
    void TogglePadInvertY();
    FString InputHint(const FString& Keyboard, const FString& Xbox, const FString& Sony) const;
    void BackFromMenu();
    bool SaveNow();
    void Notify(const FString& Text, double Seconds = 7);
    bool Tick(float Seconds);
    FString RegionText() const;
    FString NearbyText() const;
    FString StatusText() const;
    FString DiagnosticsText() const;
    FString ResolutionText() const;
    bool CanContinue() const;
    bool CanPlay() const;
    TArray<EW::PlaceBookmark> JournalPage(int32 Page) const;
    int32 RecordCount() const;
    bool IsRecorded(const EW::PlaceBookmark& Place) const;
    EEWMenu Menu() const { return CurrentMenu; }
    bool SessionStarted() const { return bSessionStarted; }
    bool IsDesktopHUDActive() const { return NativeHUD.IsVisible(); }
    FString DesktopHUDEvidence() const { return NativeHUD.Evidence(); }
    bool IsHighQuality() const { return bHighQuality; }
    bool IsSoftStyle() const { return bSoftStyle; }
    FEWSaveStore* Store() const { return Saves.Get(); }
    UPROPERTY() TObjectPtr<AEWChunkManager> Manager;
    UPROPERTY() TObjectPtr<AEWWaterView> WaterView;
    UPROPERTY() TObjectPtr<AEWMediaScreen> MediaScreen;
    UPROPERTY() TObjectPtr<AEWMediaScreen> CinemaScreen;
    UPROPERTY() TObjectPtr<AEWMediaScreen> SkyTheatre;
    UPROPERTY() TObjectPtr<AEWAirship> Airship;
    UPROPERTY() TArray<TObjectPtr<AEWAirship>> Airships;
    UPROPERTY() TArray<TObjectPtr<AEWAeroYacht>> Yachts;
    UPROPERTY() TObjectPtr<AEWAeroPort> Skyport;
    UPROPERTY() TObjectPtr<AEWMediaScreen> ActiveMediaScreen;
    UPROPERTY() TObjectPtr<AEWDayCycle> DayCycle;
    UPROPERTY() TObjectPtr<AEWSkyrail> Skyrail;
    UPROPERTY() TObjectPtr<AEWSkyrail> SkyrailUpper;
    UPROPERTY() TObjectPtr<AEWCinemaSession> CinemaSession;
    UPROPERTY() TObjectPtr<AEWSocialSession> SocialSession;
    UPROPERTY() TObjectPtr<AEWFishing> Fishing;
    UPROPERTY() TObjectPtr<AEWPhotoMode> PhotoMode;
    UPROPERTY() TObjectPtr<AEWConceptRuntime> Concepts;
    UPROPERTY() TObjectPtr<AEWTerminal> Terminal;
    UPROPERTY() TObjectPtr<AEWMusic> Music;
    UPROPERTY() TWeakObjectPtr<AEWLift> ActiveLift;
    bool bDiagnostics = false;
    FEWGraphics Graphics;
    bool bUsingGamepad = false, bSonyGamepad = false, bInvertPadY = false;
    bool bScriptedWorldAudit = false;
    float PadLookSpeed = 120.f;
    uint64 GamepadEvents = 0;
    int32 JournalPageIndex = 0, WorldPageIndex = 0;
private:
    FEWHudLayer NativeHUD;
    TUniquePtr<FEWSaveStore> Saves;
    FTSTicker::FDelegateHandle TickerHandle;
    FDelegateHandle WorldTearDownHandle;
    FDelegateHandle DisplayMetricsHandle;
    FString DisplayTopology;
    double PendingDisplayRestore = -1;
    int32 DisplayRestoreCount = 0;
    EEWMenu CurrentMenu = EEWMenu::Main;
    bool bSessionStarted = false, bHighQuality = true, bDiscardExit = false;
    bool bSoftStyle = true;
    FString Message;
    FString InputEvidencePath;
    double MessageUntil = 0, LastSaveTime = 0;
    EW::PlaceBookmark LastSaved;
    TOptional<EW::PlaceBookmark> LastKnownPosition;
    bool bWorldEnding = false;
    void CacheCurrentPosition();
    void BeforeWorldTearDown(UWorld* World);
    void DisplayMetricsChanged(const struct FDisplayMetrics& Metrics);
    void Start(const EW::WorldDescriptor& World, const EW::PlaceBookmark& Place);
    EW::PlaceBookmark StartPlace(const EW::WorldDescriptor& World) const;
};
