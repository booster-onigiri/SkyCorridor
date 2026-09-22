#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWMusic.generated.h"
class UAudioComponent;
class USoundWave;

UCLASS()
class ENDLESSWORLD_API AEWMusic : public AActor
{
    GENERATED_BODY()
public:
    AEWMusic();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void SetVolume(float Value);
    float Volume() const { return UserVolume; }
    FString Title() const;
    TSharedRef<class FJsonObject> Evidence() const;
    UFUNCTION(BlueprintCallable) void PreviewCue(int32 Index);
private:
    UPROPERTY() TObjectPtr<UAudioComponent> Audio;
    UPROPERTY() TArray<TObjectPtr<USoundWave>> Tracks;
    float UserVolume=.32f, Gain=0;
    double Remaining=32, PlayingFor=0, DuckHold=0;
    int32 Current=INDEX_NONE, Next=0, Started=0;
    bool bSilent=false;
    void StartCue(int32 Index);
};
