﻿#include "ManagedActors.h"

#include "ManagerSubsystem.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


TAutoConsoleVariable<float> CVarDynamicMoveRange(
    TEXT("dynamic.range"),
    0.01f,
    TEXT("Dynamic actor move range"),
    ECVF_Default
);
TAutoConsoleVariable<float> CVarDynamicSpeed(
    TEXT("dynamic.speed"),
    0.005f,
    TEXT("Dynamic actor move speed"),
    ECVF_Default
);


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
static FVector2f FastRandomF2(){
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

        ID = Subsystem->Tree.CreateProxy(WorldBound, this);
        ensure(ID != -1);

        Subsystem->Whole.Emplace(this);
    }
    while(false);
}
void AManagedActor::EndPlay(const EEndPlayReason::Type EndPlayReason){
    if(TObjectPtr<UStaticMeshComponent> Component = GetStaticMeshComponent(); IsValid(Component))
        Component->SetMaterial(0, nullptr);
    
    do{
        const UWorld* World = GetWorld();
        if(!World)
            break;

        UManagerSubsystem* Subsystem = World->GetSubsystem<UManagerSubsystem>();
        if(!Subsystem)
            break;

        ensure(ID != -1);
        Subsystem->Tree.DestroyProxy(ID);

        Subsystem->Whole.Remove(this);
    }
    while(false);
    
    Super::EndPlay(EndPlayReason);
}


void AManagedActor::ChangeMaterial(){
    TObjectPtr<UStaticMeshComponent> Component = GetStaticMeshComponent();
    if(!IsValid(Component))
        return;

    Component->SetMaterial(0, ((FlagState & 1) == 1) ? DetectedMaterial : NormalMaterial);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void ADynamicActor::BeginPlay(){
    Super::BeginPlay();

    {
        const FVector Extents = (LocalBound.Upper - LocalBound.Lower) * 0.5;
        
        Radius = static_cast<decltype(Radius)>((FastRandomF() + 1.f) * FMath::Max3(Extents.X, Extents.Y, Extents.Z));
        Mover = FastRandomF() * UE_PI;
    }
    
    do{
        const UWorld* World = GetWorld();
        if(!World)
            break;

        UManagerSubsystem* Subsystem = World->GetSubsystem<UManagerSubsystem>();
        if(!Subsystem)
            break;

        Subsystem->Dynamics.Emplace(this);
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
    Mover += DeltaSeconds * UE_PI * CVarDynamicSpeed.GetValueOnGameThread();
    Mover = FMath::Fmod(Mover, 2 * UE_PI);
    
    float SinVal, CosVal;
    FMath::SinCos(&SinVal, &CosVal, Mover);
    const float Range = CVarDynamicMoveRange.GetValueOnGameThread() * Radius;
    SinVal *= Range;
    CosVal *= Range;
    
    FVector CurPos = GetActorLocation();
    CurPos.X += CosVal;
    CurPos.Y += SinVal;
    SetActorLocation(CurPos, false, nullptr, ETeleportType::TeleportPhysics);
    CurPos = GetActorLocation();
    
    WorldBound = decltype(WorldBound)(LocalBound.Lower + CurPos, LocalBound.Upper + CurPos);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

