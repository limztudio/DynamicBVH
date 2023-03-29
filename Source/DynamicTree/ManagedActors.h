#pragma once


#include "CoreMinimal.h"
#include "DynamicBVH.hpp"

#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "ManagedActors.generated.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


UCLASS()
class DYNAMICTREE_API AManagedActor : public AStaticMeshActor{
    GENERATED_BODY()


public:
    virtual void BeginPlay()override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason)override;

public:
    void ClearFlag(){ FlagState = 0; }
    void SetFlag(uint8 Flag){ FlagState |= Flag; }
    
    void ChangeColour();
    
    const TBVHBound<double>& GetWorldBound()const{ return WorldBound; }
    int32 GetID()const{ return ID; }

    
protected:
    TBVHBound<double> LocalBound, WorldBound;
    int32 ID = -1;

private:
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> Material;

    uint8 FlagState = 0;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


UCLASS()
class DYNAMICTREE_API AStaticActor : public AManagedActor{
    GENERATED_BODY()
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


UCLASS()
class DYNAMICTREE_API ADynamicActor : public AManagedActor{
    GENERATED_BODY()


public:
    virtual void BeginPlay()override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason)override;

public:
    void Update(float DeltaSeconds);


private:
    FVector RandomDirection;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

