#pragma once

#include "CoreMinimal.h"

#include "Tickable.h"
#include "Subsystems/WorldSubsystem.h"

#include "DynamicBVH.hpp"

#include "ManagerSubsystem.generated.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class AManagedActor;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


UCLASS()
class DYNAMICTREE_API UManagerSubsystem : public UWorldSubsystem, public FTickableGameObject{
    GENERATED_BODY()


public:
    virtual void Initialize(FSubsystemCollectionBase& Collection)override;
    virtual void Deinitialize()override;


public:
#if UE_EDITOR
    virtual ETickableTickType GetTickableTickType()const override;
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType)const override;
    virtual bool IsTickableInEditor()const override;
#endif

    virtual UWorld* GetTickableGameObjectWorld()const override;
    virtual TStatId GetStatId()const override;

    virtual void Tick(float DeltaSeconds)override;


private:
    TDynamicBVH<AManagedActor*> Tree;


public:
    friend AManagedActor;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

