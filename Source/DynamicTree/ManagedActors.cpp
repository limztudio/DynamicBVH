#include "ManagedActors.h"
#include "ManagerSubsystem.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


extern TAutoConsoleVariable<bool> CVarManagerUseBVH;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static const FName NameOpaque(TEXT("Opaque"));


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void AManagedActor::BeginPlay(){
    Super::BeginPlay();

    if(TObjectPtr<UStaticMeshComponent> Component = GetStaticMeshComponent(); IsValid(Component))
        Material = Component->CreateAndSetMaterialInstanceDynamic(0);

    do{
        const UWorld* World = GetWorld();
        if(!World)
            break;

        UManagerSubsystem* Subsystem = World->GetSubsystem<UManagerSubsystem>();
        if(!Subsystem)
            break;

        const FVector InitialPos = GetActorLocation();
        {
            WorldBound = GetComponentsBoundingBox(true, true);
            LocalBound = decltype(LocalBound)(WorldBound.Lower - InitialPos, WorldBound.Upper - InitialPos);
        }
        
        if(CVarManagerUseBVH.GetValueOnGameThread()){
            ID = Subsystem->Tree.CreateProxy(WorldBound, this);
            ensure(ID != -1);
        }
        else{
            Subsystem->Whole.AddUnique(this);
        }
    }
    while(false);
}
void AManagedActor::EndPlay(const EEndPlayReason::Type EndPlayReason){
    Material = nullptr;
    if(TObjectPtr<UStaticMeshComponent> Component = GetStaticMeshComponent(); IsValid(Component))
        Component->SetMaterial(0, nullptr);
    
    do{
        const UWorld* World = GetWorld();
        if(!World)
            break;

        UManagerSubsystem* Subsystem = World->GetSubsystem<UManagerSubsystem>();
        if(!Subsystem)
            break;

        if(CVarManagerUseBVH.GetValueOnGameThread()){
            ensure(ID != -1);
            Subsystem->Tree.DestroyProxy(ID);
        }
        else{
            Subsystem->Whole.Remove(this);
        }
    }
    while(false);
    
    Super::EndPlay(EndPlayReason);
}


void AManagedActor::ChangeColour(){
    if(!IsValid(Material))
        return;
    
    Material->SetVectorParameterValue(NameOpaque, ((FlagState & 1) == 1) ? FLinearColor::Green : FLinearColor::Red);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void ADynamicActor::BeginPlay(){
    Super::BeginPlay();
    
    do{
        const UWorld* World = GetWorld();
        if(!World)
            break;

        UManagerSubsystem* Subsystem = World->GetSubsystem<UManagerSubsystem>();
        if(!Subsystem)
            break;

        Subsystem->Dynamics.AddUnique(this);
    }
    while(false);
}
void ADynamicActor::EndPlay(const EEndPlayReason::Type EndPlayReason){
    do{
        const UWorld* World = GetWorld();
        if(!World)
            break;

        UManagerSubsystem* Subsystem = World->GetSubsystem<UManagerSubsystem>();
        if(!Subsystem)
            break;

        Subsystem->Dynamics.Remove(this);
    }
    while(false);
    
    Super::EndPlay(EndPlayReason);
}


void ADynamicActor::Update(float DeltaSeconds){
    const FVector CurPos = GetActorLocation();
    WorldBound = decltype(WorldBound)(LocalBound.Lower + CurPos, LocalBound.Upper + CurPos);    
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

