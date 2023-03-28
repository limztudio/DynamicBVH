#include "ManagedActors.h"
#include "ManagerSubsystem.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


extern TAutoConsoleVariable<bool> CVarManagerUseBVH;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void AManagedActor::BeginPlay(){
    Super::BeginPlay();

    do{
        if(!CVarManagerUseBVH.GetValueOnGameThread())
            break;
        
        const UWorld* World = GetWorld();
        if(!World)
            break;

        UManagerSubsystem* Subsystem = World->GetSubsystem<UManagerSubsystem>();
        if(!Subsystem)
            break;
        
        ID = Subsystem->Tree.CreateProxy(GetComponentsBoundingBox(true, true));
        ensure(ID != -1);
    }
    while(false);
}
void AManagedActor::EndPlay(const EEndPlayReason::Type EndPlayReason){
    do{
        if(!CVarManagerUseBVH.GetValueOnGameThread())
            break;
        
        const UWorld* World = GetWorld();
        if(!World)
            break;

        UManagerSubsystem* Subsystem = World->GetSubsystem<UManagerSubsystem>();
        if(!Subsystem)
            break;

        ensure(ID != -1);
        Subsystem->Tree.DestroyProxy(ID);
    }
    while(false);
    
    Super::EndPlay(EndPlayReason);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

