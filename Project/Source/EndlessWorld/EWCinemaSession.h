#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "EWCinemaSession.generated.h"
class IWebSocket;
class UStaticMeshComponent;
class UWidgetComponent;
class UStaticMesh;

UCLASS()
class ENDLESSWORLD_API AEWCinemaPeer:public AActor
{
    GENERATED_BODY()
public:
    AEWCinemaPeer();
    virtual void BeginPlay() override;
    void UpdatePose(const FTransform& Frame,FVector Position,float Yaw,bool Seated,bool Visible,const FString& Name,float Delta);
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY() TObjectPtr<UWidgetComponent> Nameplate;
    UPROPERTY() TObjectPtr<UStaticMesh> StandingMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> SeatedMesh;
    FString DisplayName;
    bool bPlaced=false;
};

UCLASS()
class ENDLESSWORLD_API AEWCinemaSession:public AActor
{
    GENERATED_BODY()
public:
    AEWCinemaSession();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void Host(const FString& Name);
    void Join(const FString& Code,const FString& Name);
    void Leave();
    bool Connected() const{return bConnected;}
    bool Hosting() const{return bHosting;}
    bool Guest() const{return bConnected && !bHosting;}
    FString Status() const{return Message;}
    FString Invite() const{return InviteCode;}
    FString Name() const{return DisplayName;}
    int32 Members() const{return PeerCount;}
    int32 Seat() const{return HeldSeat;}
    void ReserveSeat(int32 Index);
    void ReleaseSeat();
    bool HasSharedHour() const{return bConnected && LastState>0;}
    double SharedHour() const;
    TSharedRef<class FJsonObject> Evidence() const;
private:
    TSharedPtr<IWebSocket> Socket;
    FProcHandle ServerProcess;
    FString InfoPath,InviteCode,DisplayName=TEXT("旅人"),Message=TEXT("一人で鑑賞中"),SelfId;
    bool bConnected=false,bHosting=false,bConnecting=false,bEnding=false;
    int32 PeerCount=0,HeldSeat=-1,ConnectionGeneration=0,AuditSequence=0;
    FString AuditControl;
    double AuditStarted=0;
    double Started=0,LastPose=0,LastMedia=0,LastPing=0,LastState=0,ServerHour=0,RTT=0,LastWelcome=0,LastInfoRead=0;
    double SyncError=0,ServerDaySeconds=3600;
    TSharedPtr<class FJsonObject> State;
    UPROPERTY() TMap<FString,TObjectPtr<AEWCinemaPeer>> Avatars;
    void Connect(const FString& Endpoint,const FString& Key,bool HostRole);
    void Receive(const FString& Text);
    void Send(const TSharedRef<class FJsonObject>& Json);
    void TickAudit();
};
