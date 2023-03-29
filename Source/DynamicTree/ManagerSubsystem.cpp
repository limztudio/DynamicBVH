#include "ManagerSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

#include "ManagedActors.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) || USE_LOGGING_IN_SHIPPING
#define LEAVE_STAT
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


TAutoConsoleVariable<float> CVarManagerDetectRadius(
    TEXT("manager.radius"),
    50000.f,
    TEXT("Detect distance from view location"),
    ECVF_Default
);
TAutoConsoleVariable<bool> CVarManagerUseBVH(
    TEXT("manager.usebvh"),
    true,
    TEXT("Use dynamic tree for managing actors"),
    ECVF_Default
);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifdef LEAVE_STAT
static const FName NameLogAvg(TEXT("AvgSearch"));
static const FName NameLogMax(TEXT("MaxSearch"));
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static bool IntersectSphereAndAABB(VectorRegister XMMSphereCentre, VectorRegister XMMSSphereRadius, VectorRegister XMMBoxLower, VectorRegister XMMBoxUpper){
    VectorRegister XMMTmp;

    VectorRegister XMMClosest;
    {
        XMMTmp = VectorCompareGE(XMMSphereCentre, XMMBoxUpper);
        XMMClosest = VectorSelect(XMMTmp, XMMBoxUpper, XMMSphereCentre);
    
        XMMTmp = VectorCompareLE(XMMSphereCentre, XMMBoxLower);
        XMMClosest = VectorSelect(XMMTmp, XMMBoxLower, XMMClosest);
    }

    XMMTmp = VectorSubtract(XMMClosest, XMMSphereCentre);
    XMMTmp = VectorDot3(XMMTmp, XMMTmp);

    XMMSSphereRadius = VectorMultiply(XMMSSphereRadius, XMMSSphereRadius);

    XMMTmp = VectorCompareLT(XMMTmp, XMMSSphereRadius);
    return (VectorMaskBits(XMMTmp) & 0x01) != 0;
}


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

    bool bHaValidViewLocation = false;
    FVector4 ViewLocation;

#if WITH_EDITOR
    if(GEditor){
        bool bIsPlaying = World->HasBegunPlay();
        if(bIsPlaying)
            bIsPlaying = !GEditor->bIsSimulatingInEditor;

        if(bIsPlaying){
            do{
                const TObjectPtr<APlayerController> PlayerControl = World->GetFirstPlayerController();
                if(!IsValid(PlayerControl))
                    continue;

                const TObjectPtr<ULocalPlayer> Player = Cast<ULocalPlayer>(PlayerControl->Player);
                if(!IsValid(Player))
                    continue;

                const TObjectPtr<UGameViewportClient> ViewportClient = Player->ViewportClient;
                if(!IsValid(ViewportClient))
                    continue;

                FViewport* Viewport = ViewportClient->Viewport;
                if(!Viewport)
                    continue;

                if(Viewport->GetSizeXY().Size() <= 0)
                    continue;

                FSceneViewFamilyContext ViewFamily(
                    FSceneViewFamily::ConstructionValues(
                        Viewport,
                        World->Scene,
                        ViewportClient->EngineShowFlags
                    ).SetRealtimeUpdate(true)
                );

                FVector NewLocation;
                FRotator NewRotation;
                if(const FSceneView* View = Player->CalcSceneView(&ViewFamily, NewLocation, NewRotation, Viewport)){
                    const int32 PlaneCount = View->ViewFrustum.PermutedPlanes.Num();
                    if(!(PlaneCount % 4)){
                        bHaValidViewLocation = true;
                        ViewLocation = View->ViewLocation;
                    }
                }
            }
            while(false);
        }
        else{
            for(FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients()){
                if(!ViewportClient)
                    continue;

                if(ViewportClient->GetWorld() != World)
                    continue;

                const FViewport* Viewport = ViewportClient->Viewport;
                if(!Viewport)
                    continue;

                if(Viewport->GetSizeXY().Size() <= 0)
                    continue;

                FSceneViewFamilyContext ViewFamily(FSceneViewFamilyContext::ConstructionValues(
                    Viewport,
                    World->Scene,
                    ViewportClient->EngineShowFlags
                ).SetRealtimeUpdate(ViewportClient->IsRealtime()));

                if(const FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily)){
                    const int32 PlaneCount = View->ViewFrustum.PermutedPlanes.Num();
                    if((PlaneCount % 4))
                        continue;

                    bHaValidViewLocation = true;
                    ViewLocation = View->ViewLocation;
                }
                break;
            }
        }
    }
    else{
#endif
        do{
            const TObjectPtr<APlayerController> PlayerControl = World->GetFirstPlayerController();
            if(!IsValid(PlayerControl))
                continue;

            const TObjectPtr<ULocalPlayer> Player = Cast<ULocalPlayer>(PlayerControl->Player);
            if(!IsValid(Player))
                continue;

            const TObjectPtr<UGameViewportClient> ViewportClient = Player->ViewportClient;
            if(!IsValid(ViewportClient))
                continue;

            FViewport* Viewport = ViewportClient->Viewport;
            if(!Viewport)
                continue;

            if(Viewport->GetSizeXY().Size() <= 0)
                continue;

            FSceneViewFamilyContext ViewFamily(
                FSceneViewFamily::ConstructionValues(
                    Viewport,
                    World->Scene,
                    ViewportClient->EngineShowFlags
                ).SetRealtimeUpdate(true)
            );

            FVector NewLocation;
            FRotator NewRotation;
            if(FSceneView* View = Player->CalcSceneView(&ViewFamily, NewLocation, NewRotation, Viewport)){
                int32 PlaneCount = View->ViewFrustum.PermutedPlanes.Num();
                if(!(PlaneCount % 4)){
                    bHaValidViewLocation = true;
                    ViewLocation = View->ViewLocation;
                }
            }
        }
        while(false);
#if WITH_EDITOR
    }
#endif

    if(CVarManagerUseBVH.GetValueOnGameThread()){
        for(auto& Wrap : Tree){
            auto& Actor = Wrap.Get<1>();
            if(!Actor.IsValid())
                continue;

            Actor->ClearFlag();
        }
    }
    else{
        for(auto& Actor : Whole){
            if(!Actor.IsValid())
                continue;

            Actor->ClearFlag();
        }
    }
    
    if(CVarManagerUseBVH.GetValueOnGameThread()){
        for(TWeakObjectPtr<ADynamicActor>& Actor : Dynamics){
            if(!Actor.IsValid())
                continue;

            Actor->Update(DeltaSeconds);
            Tree.MoveProxy(Actor->GetID(), Actor->GetWorldBound());
        }
    }
    else{
        for(TWeakObjectPtr<ADynamicActor>& Actor : Dynamics){
            if(!Actor.IsValid())
                continue;

            Actor->Update(DeltaSeconds);
        }    
    }

    if(bHaValidViewLocation){
        const float DetectRadius = CVarManagerDetectRadius.GetValueOnGameThread();

#ifdef LEAVE_STAT
        int32 MaxSearchCount = 0;
        int32 AvgSearchCount = 0;
        int32 FrameCount = 0;
#endif
        
        if(CVarManagerUseBVH.GetValueOnGameThread()){
#ifdef LEAVE_STAT
            Tree.DebugQuery(
#else
            Tree.Query(
#endif
                [DetectRadius, &ViewLocation](const TBVHBound<double>& CurBound){
                    return IntersectSphereAndAABB(
                        VectorLoadAligned(&ViewLocation),
                        VectorSetDouble1(DetectRadius),
                        VectorLoadAligned(&CurBound.Lower),
                        VectorLoadAligned(&CurBound.Upper)
                        );
                },
                [
#ifdef LEAVE_STAT
                    &MaxSearchCount
                    , &AvgSearchCount
                    , &FrameCount
#endif
                    ](
                        int32
                        , TWeakObjectPtr<AManagedActor>& Actor
#ifdef LEAVE_STAT
                        , int32 Depth
#endif
                        ){
#ifdef LEAVE_STAT
                    MaxSearchCount = FMath::Max(MaxSearchCount, Depth);
                    AvgSearchCount += Depth;
                    ++FrameCount;
#endif   
                    if(!Actor.IsValid())
                        return;

                    Actor->SetFlag(1);
                }
            );
        }
        else{
#ifdef LEAVE_STAT
            int32 SearchCount = 0;
#endif
            for(TWeakObjectPtr<AManagedActor>& Actor : Whole){
#ifdef LEAVE_STAT
                ++SearchCount;
#endif
                if(!Actor.IsValid())
                    continue;

                const auto& CurBound = Actor->GetWorldBound();
                if(!IntersectSphereAndAABB(
                    VectorLoadAligned(&ViewLocation),
                    VectorSetDouble1(DetectRadius),
                    VectorLoadAligned(&CurBound.Lower),
                    VectorLoadAligned(&CurBound.Upper)
                ))
                    continue;

#ifdef LEAVE_STAT
                MaxSearchCount = FMath::Max(MaxSearchCount, SearchCount);
                AvgSearchCount += SearchCount;
                ++FrameCount;
#endif

                Actor->SetFlag(1);
            }
        }

#ifdef LEAVE_STAT
        {
            float AvgSearchCountF = AvgSearchCount;
            if(FrameCount > 0)
                AvgSearchCountF /= FrameCount;

            UKismetSystemLibrary::PrintString(nullptr, FString::Printf(TEXT("Avg. searching count: %f"), AvgSearchCountF), true, false, FLinearColor::Blue, 2.f, NameLogAvg);
            UKismetSystemLibrary::PrintString(nullptr, FString::Printf(TEXT("Max. searching count: %d"), MaxSearchCount), true, false, FLinearColor::Yellow, 2.f, NameLogMax);
        }
#endif
    }

    if(CVarManagerUseBVH.GetValueOnGameThread()){
        for(auto& Wrap : Tree){
            auto& Actor = Wrap.Get<1>();
            if(!Actor.IsValid())
                continue;

            Actor->ChangeMaterial();
        }
    }
    else{
        for(auto& Actor : Whole){
            if(!Actor.IsValid())
                continue;

            Actor->ChangeMaterial();
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifdef LEAVE_STAT
#undef LEAVE_STAT
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

