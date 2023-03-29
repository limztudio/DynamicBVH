#include "ManagedActors.h"
#include "ManagerSubsystem.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#define MOVE_SPEED 3


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


extern TAutoConsoleVariable<bool> CVarManagerUseBVH;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static const FName NameOpaque(TEXT("Opaque"));


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static uint64 FastRandomU64(){
    // xorshift*

    thread_local uint64 X = 1; // initial seed must be nonzero
    X ^= X >> 12;
    X ^= X << 25;
    X ^= X >> 27;
    return X * 0x2545f4914f6cdd1dULL;
}
static float FastRandomF(){
    return static_cast<float>(((FastRandomU64() >> 8) & 0x00ffffff) / 16777216.f);
}
static FVector2f TAFastRandomF2(){
    union{
        uint64 U64;
        struct{
            uint32 U32[2];
        };
    } V = { FastRandomU64() };
    return FVector2f(static_cast<float>(V.U32[0] & 0x00ffffff), static_cast<float>(V.U32[1] & 0x00ffffff)) / 16777216.f;
}
static FVector3f FastRandomF3(){
    union{
        uint64 U64;
        struct{
            uint64 U64_0 : 21;
            uint64 U64_1 : 22;
            uint64 U64_2 : 21;
        };
    } V = { FastRandomU64() };
    return FVector3f(static_cast<float>(V.U64_0), static_cast<float>(V.U64_1), static_cast<float>(V.U64_2)) / FVector3f(2097151.f, 4194303.f, 2097151.f);
}
static FVector4f FastRandomF4(){
    union{
        uint64 U64;
        struct{
            uint16 U16[4];
        };
    } V = { FastRandomU64() };
    return FVector4f(static_cast<float>(V.U16[0]), static_cast<float>(V.U16[1]), static_cast<float>(V.U16[2]), static_cast<float>(V.U16[3])) / FVector4f(65535.f, 65535.f, 65535.f, 65535.f);
}


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

    {
        const auto Rand = FastRandomF3();
        
        RandomDirection.X = Rand.X;
        RandomDirection.Y = Rand.Y;
        RandomDirection.Z = Rand.Z;
        RandomDirection = RandomDirection.GetSafeNormal();
    }
    
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
    FVector CurPos = GetActorLocation();
    CurPos += RandomDirection * DeltaSeconds * MOVE_SPEED;
    SetActorLocation(CurPos, false, nullptr, ETeleportType::TeleportPhysics);
    CurPos = GetActorLocation();
    
    WorldBound = decltype(WorldBound)(LocalBound.Lower + CurPos, LocalBound.Upper + CurPos);    
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#undef MOVE_SPEED


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

