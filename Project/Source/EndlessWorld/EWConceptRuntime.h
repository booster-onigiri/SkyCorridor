#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWWorkshopData.h"
#include "EWChessRules.h"
#include "EWConceptRuntime.generated.h"

class UStaticMesh; class UStaticMeshComponent; class UMaterialInterface; class UMaterialInstanceDynamic; class UBoxComponent;
UCLASS()
class ENDLESSWORLD_API AEWConceptRuntime : public AActor
{
    GENERATED_BODY()
public:
    AEWConceptRuntime();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    FString Directory() const;
    TArray<FString> Files(bool Eggs) const;
    bool LoadEgg(const FString& File);
    bool Add(const FString& File);
    void Remove();
    bool Nearby() const;
    bool Interact();
    bool Active() const{return bActive;}
    const EWWorkshop::Concept& Package() const{return Definition;}
    const EWChess::Game& Match() const{return Game;}
    bool Play(const FString& Move);
    bool Undo();
    bool NewMatch();
    bool ClaimDraw();
    FString MatchStatus() const;
    FString LastMessage() const{return Message;}
    TSharedRef<class FJsonObject> Evidence() const;
    UFUNCTION(BlueprintCallable) FString WorkshopAction(const FString& Action,const FString& Value);
private:
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UStaticMesh> Cone;
    UPROPERTY() TObjectPtr<UMaterialInterface> Surface;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Shapes;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> PieceShapes;
    UPROPERTY() TObjectPtr<UBoxComponent> Body;
    EWWorkshop::Concept Definition;
    EWChess::Game Game;
    EW::ChunkCoord AnchorChunk;
    FVector AnchorLocal=FVector::ZeroVector;
    double AnchorYaw=0;
    FString WorldCode,Message,LoadedEgg,LoadedDigest;
    bool bActive=false;
    bool LocalOnly();
    bool Tell(bool OK,const FString& Text);
    bool Commit(const EWChess::Game& Next);
    bool ReadMatch(const FString& Id,EWChess::Game& Out);
    FString MatchPath(const FString& Id) const;
    bool Placement(double Size,FVector& At,double& Yaw) const;
    void Build();
    void UpdatePieces();
    void Clear();
    UStaticMeshComponent* Shape(UStaticMesh* Mesh,FVector P,FVector Size,int32 Material,bool Piece=false,FRotator Q=FRotator::ZeroRotator);
};
