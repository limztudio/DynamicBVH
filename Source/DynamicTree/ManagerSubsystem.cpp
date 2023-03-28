#include "ManagerSubsystem.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


TAutoConsoleVariable<bool> CVarManagerUseBVH(
    TEXT("manager.usebvh"),
    true,
    TEXT("Use dynamic tree for managing actors"),
    ECVF_Default
);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void UManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection){
}
void UManagerSubsystem::Deinitialize(){
}


#if UE_EDITOR
ETickableTickType UManagerSubsystem::GetTickableTickType()const{
    return ETickableTickType::Always;
}
bool UManagerSubsystem::DoesSupportWorldType(EWorldType::Type WorldType)const{
    switch(WorldType){
    case EWorldType::Game:
    case EWorldType::PIE:
        return true;

    default:
        return false;
    }
}
bool UManagerSubsystem::IsTickableInEditor()const{
    const UWorld* World = GetWorld();
    if(!World)
        return false;

    return true;
}
#endif

UWorld* UManagerSubsystem::GetTickableGameObjectWorld()const{
    return GetWorld();
}
TStatId UManagerSubsystem::GetStatId()const{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UManagerSubsystem, STATGROUP_Tickables);
}

void UManagerSubsystem::Tick(float DeltaSeconds){
    UWorld* World = GetWorld();
    if(!World)
        return;
	
#if UE_EDITOR
    if(World->bDebugPauseExecution)
        DeltaSeconds = 0.f;
#endif

    
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
