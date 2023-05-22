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
#include <chrono>
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
static const FName NameLogWholeTimeTaken(TEXT("WholeTimeTaken"));
static const FName NameLogLocalTimeTaken0(TEXT("LocalTimeTaken0"));
static const FName NameLogLocalTimeTaken1(TEXT("LocalTimeTaken1"));
static const FName NameLogLocalTimeTaken2(TEXT("LocalTimeTaken2"));
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


template<bool bCheckIfFullyContained>
static FORCEINLINE uint8 IntersectSphereWithAABB(VectorRegister XMMSphereCentre, VectorRegister XMMSSphereRadius, VectorRegister XMMBoxLower, VectorRegister XMMBoxUpper){
    VectorRegister XMMTmp;
    VectorRegister XMMClosest;

    VectorRegister XMMSSphereRadiusSq = VectorMultiply(XMMSSphereRadius, XMMSSphereRadius);

    if(bCheckIfFullyContained){
        static const VectorRegister XMMMask1010 = MakeVectorRegisterDoubleMask(~uint64(0), uint64(0), ~uint64(0), uint64(0));
        static const VectorRegister XMMMask0110 = MakeVectorRegisterDoubleMask(uint64(0), ~uint64(0), ~uint64(0), uint64(0));
        static const VectorRegister XMMMask1000 = MakeVectorRegisterDoubleMask(~int64(0), uint64(0), uint64(0), uint64(0));
        static const VectorRegister XMMMask0100 = MakeVectorRegisterDoubleMask(int64(0), ~uint64(0), uint64(0), uint64(0));
        
        VectorRegister XMMVerts[] = {
            XMMBoxLower,
            VectorShuffle(XMMBoxLower, XMMBoxUpper, 0, 1, 2, 3),
            VectorSelect(XMMMask1010, XMMBoxLower, XMMBoxUpper),
            VectorSelect(XMMMask0110, XMMBoxLower, XMMBoxUpper),
            VectorSelect(XMMMask1000, XMMBoxLower, XMMBoxUpper),
            VectorSelect(XMMMask0100, XMMBoxLower, XMMBoxUpper),
            VectorShuffle(XMMBoxUpper, XMMBoxLower, 0, 1, 2, 3),
            XMMBoxUpper,
        };

        for(const auto& XMMVert : XMMVerts){
            XMMTmp = VectorSubtract(XMMSphereCentre, XMMVert);
            XMMTmp = VectorDot3(XMMTmp, XMMTmp);

            XMMTmp = VectorCompareGT(XMMTmp, XMMSSphereRadiusSq);
            if(VectorMaskBits(XMMTmp) & 0x01)
                goto CHECK_IF_INTERSECT_BUT_NOT_CONTAINED;
        }

        return 0x02;
    }

CHECK_IF_INTERSECT_BUT_NOT_CONTAINED:
    {
        XMMTmp = VectorCompareGE(XMMSphereCentre, XMMBoxUpper);
        XMMClosest = VectorSelect(XMMTmp, XMMBoxUpper, XMMSphereCentre);

        XMMTmp = VectorCompareLE(XMMSphereCentre, XMMBoxLower);
        XMMClosest = VectorSelect(XMMTmp, XMMBoxLower, XMMClosest);
    }

    XMMTmp = VectorSubtract(XMMClosest, XMMSphereCentre);
    XMMTmp = VectorDot3(XMMTmp, XMMTmp);

    XMMTmp = VectorCompareLT(XMMTmp, XMMSSphereRadiusSq);
    if(VectorMaskBits(XMMTmp) & 0x01)
        return 0x01;

    return 0x00;
}

template<bool bCheckIfFullyContained>
static FORCEINLINE uint8 IntersectFrustumWithAABB(const FConvexVolume::FPermutedPlaneArray& PermutedPlanes, VectorRegister XMMBoxLower, VectorRegister XMMBoxUpper){
    bool bContained = true;
    
    VectorRegister XMMBoxOrigin = VectorAdd(XMMBoxUpper, XMMBoxLower);
    XMMBoxOrigin = VectorMultiply(XMMBoxOrigin, GlobalVectorConstants::DoubleOneHalf);
    VectorRegister XMMBoxExtent = VectorSubtract(XMMBoxUpper, XMMBoxOrigin);
    
    // Splat origin into 3 vectors
    VectorRegister XMMOrigX = VectorReplicate(XMMBoxOrigin, 0);
    VectorRegister XMMOrigY = VectorReplicate(XMMBoxOrigin, 1);
    VectorRegister XMMOrigZ = VectorReplicate(XMMBoxOrigin, 2);
    // Splat extent into 3 vectors
    VectorRegister XMMExtentX = VectorReplicate(XMMBoxExtent, 0);
    VectorRegister XMMExtentY = VectorReplicate(XMMBoxExtent, 1);
    VectorRegister XMMExtentZ = VectorReplicate(XMMBoxExtent, 2);
    // Since we are moving straight through get a pointer to the data
    const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)PermutedPlanes.GetData();
    
    // Process four planes at a time until we have < 4 left
    for(int32 Count = 0, Num = PermutedPlanes.Num(); Count < Num; Count += 4){
        // Load 4 planes that are already all Xs, Ys, ...
        VectorRegister XMMPlanesX = VectorLoadAligned(PermutedPlanePtr);
        PermutedPlanePtr++;
        VectorRegister XMMPlanesY = VectorLoadAligned(PermutedPlanePtr);
        PermutedPlanePtr++;
        VectorRegister XMMPlanesZ = VectorLoadAligned(PermutedPlanePtr);
        PermutedPlanePtr++;
        VectorRegister XMMPlanesW = VectorLoadAligned(PermutedPlanePtr);
        PermutedPlanePtr++;
        // Calculate the distance (x * x) + (y * y) + (z * z) - w
        VectorRegister XMMDistX = VectorMultiply(XMMOrigX, XMMPlanesX);
        VectorRegister XMMDistY = VectorMultiplyAdd(XMMOrigY, XMMPlanesY, XMMDistX);
        VectorRegister XMMDistZ = VectorMultiplyAdd(XMMOrigZ, XMMPlanesZ, XMMDistY);
        VectorRegister XMMDistance = VectorSubtract(XMMDistZ, XMMPlanesW);
        // Now do the push out FMath::Abs(x * x) + FMath::Abs(y * y) + FMath::Abs(z * z)
        VectorRegister XMMPushX = VectorMultiply(XMMExtentX, VectorAbs(XMMPlanesX));
        VectorRegister XMMPushY = VectorMultiplyAdd(XMMExtentY, VectorAbs(XMMPlanesY), XMMPushX);
        VectorRegister XMMPushOut = VectorMultiplyAdd(XMMExtentZ, VectorAbs(XMMPlanesZ), XMMPushY);

        if(bCheckIfFullyContained){
            // Check for completely outside
            if(VectorAnyGreaterThan(XMMDistance, XMMPushOut)){
                bContained = false;
                return 0x00;
            }

            // Definitely inside frustums, but check to see if it's fully contained
            if(VectorAnyGreaterThan(XMMDistance,VectorNegate(XMMPushOut)))
                bContained = false;
        }
        else{
            // Check for completely outside
            if(VectorAnyGreaterThan(XMMDistance, XMMPushOut))
                return 0x00;
        }
    }
    
    return bCheckIfFullyContained ? (bContained ? 0x02 : 0x01) : 0x01;
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

    bool bHasValidView = false;
    FVector4 ViewLocation;
    FConvexVolume ViewFrustum;

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
                    if((PlaneCount % 4))
                        continue;
                    
                    bHasValidView = true;
                    ViewLocation = View->ViewLocation;
                    ViewFrustum = View->ViewFrustum;
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

                    bHasValidView = true;
                    ViewLocation = View->ViewLocation;
                    ViewFrustum = View->ViewFrustum;
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
                if((PlaneCount % 4))
                    continue;
                
                bHasValidView = true;
                ViewLocation = View->ViewLocation;
                ViewFrustum = View->ViewFrustum;
            }
        }
        while(false);
#if WITH_EDITOR
    }
#endif

#ifdef LEAVE_STAT
    std::chrono::steady_clock::time_point WholeTimer[2];
    std::chrono::steady_clock::time_point LocalTimer[2];

    std::chrono::duration<double, std::chrono::seconds::period> LocalTimeTaken[3];

    WholeTimer[0] = std::chrono::steady_clock::now();
#endif

    for(auto& Actor : Whole){
        if(!Actor.IsValid())
            continue;

        Actor->ClearFlag();
    }

#ifdef LEAVE_STAT
    LocalTimer[0] = std::chrono::steady_clock::now();
#endif
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
#ifdef LEAVE_STAT
    LocalTimer[1] = std::chrono::steady_clock::now();
    LocalTimeTaken[0] = LocalTimer[1] - LocalTimer[0];
#endif

    if(bHasValidView){
        const float DetectRadius = CVarManagerDetectRadius.GetValueOnGameThread();

#ifdef LEAVE_STAT
        int32 MaxSearchCount = 0;
        int32 AvgSearchCount = 0;
        int32 FrameCount = 0;
#endif

#ifdef LEAVE_STAT
        LocalTimer[0] = std::chrono::steady_clock::now();
#endif
        if(CVarManagerUseBVH.GetValueOnGameThread()){
#ifdef LEAVE_STAT
            Tree.DebugQuery(
#else
            Tree.Query(
#endif
                [DetectRadius, &ViewLocation](const TBVHBound<double>& CurBound){
                    return IntersectSphereWithAABB<true>(
                        VectorLoadAligned(&ViewLocation),
                        VectorSetDouble1(DetectRadius),
                        VectorLoadAligned(&CurBound.Lower),
                        VectorLoadAligned(&CurBound.Upper)
                        );
                },
                [DetectRadius, &ViewLocation](const TBVHBound<double>& CurBound){
                    return IntersectSphereWithAABB<false>(
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
                if(!IntersectSphereWithAABB<false>(
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
        LocalTimer[1] = std::chrono::steady_clock::now();
        LocalTimeTaken[1] = LocalTimer[1] - LocalTimer[0];
#endif

#ifdef LEAVE_STAT
        {
            float AvgSearchCountF = AvgSearchCount;
            if(FrameCount > 0)
                AvgSearchCountF /= FrameCount;

            UKismetSystemLibrary::PrintString(nullptr, FString::Printf(TEXT("Avg. searching count: %f"), AvgSearchCountF), true, false, FLinearColor::Blue, 1.f, NameLogAvg);
            UKismetSystemLibrary::PrintString(nullptr, FString::Printf(TEXT("Max. searching count: %d"), MaxSearchCount), true, false, FLinearColor::Yellow, 1.f, NameLogMax);
        }
#endif
    }

#ifdef LEAVE_STAT
    LocalTimer[0] = std::chrono::steady_clock::now();
#endif
    if(CVarManagerUseBVH.GetValueOnGameThread()){
        Tree.Query(
            [&ViewFrustum](const TBVHBound<double>& CurBound){
                return IntersectFrustumWithAABB<true>(
                    ViewFrustum.PermutedPlanes,
                    VectorLoadAligned(&CurBound.Lower),
                    VectorLoadAligned(&CurBound.Upper)
                    );
            },
            [&ViewFrustum](const TBVHBound<double>& CurBound){
                return IntersectFrustumWithAABB<false>(
                    ViewFrustum.PermutedPlanes,
                    VectorLoadAligned(&CurBound.Lower),
                    VectorLoadAligned(&CurBound.Upper)
                    );
            },
            [](int32, TWeakObjectPtr<AManagedActor>& Actor){
                if(!Actor.IsValid())
                    return;
                
                Actor->ChangeMaterial();
            }
        );
    }
    else{
        for(auto& Actor : Whole){
            if(!Actor.IsValid())
                continue;

            const auto& CurBound = Actor->GetWorldBound();
            if(!IntersectFrustumWithAABB<false>(
                ViewFrustum.PermutedPlanes,
                VectorLoadAligned(&CurBound.Lower),
                VectorLoadAligned(&CurBound.Upper)
            ))
                continue;

            Actor->ChangeMaterial();
        }
    }
#ifdef LEAVE_STAT
    LocalTimer[1] = std::chrono::steady_clock::now();
    LocalTimeTaken[2] = LocalTimer[1] - LocalTimer[0];
#endif

#ifdef LEAVE_STAT
    UKismetSystemLibrary::PrintString(nullptr, FString::Printf(TEXT("Dynamic update time taken: %lf ms"), LocalTimeTaken[0].count() * 1000), true, false, FLinearColor::White, 1.f, NameLogLocalTimeTaken0);
    UKismetSystemLibrary::PrintString(nullptr, FString::Printf(TEXT("Sphere intersection time taken: %lf ms"), LocalTimeTaken[1].count() * 1000), true, false, FLinearColor::White, 1.f, NameLogLocalTimeTaken1);
    UKismetSystemLibrary::PrintString(nullptr, FString::Printf(TEXT("Frustum intersection time taken: %lf ms"), LocalTimeTaken[2].count() * 1000), true, false, FLinearColor::White, 1.f, NameLogLocalTimeTaken2);
    
    WholeTimer[1] = std::chrono::steady_clock::now();
    UKismetSystemLibrary::PrintString(nullptr, FString::Printf(TEXT("Total time taken: %lf ms"), std::chrono::duration<double, std::chrono::seconds::period>(WholeTimer[1] - WholeTimer[0]).count() * 1000), true, false, FLinearColor::White, 1.f, NameLogWholeTimeTaken);
#endif
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifdef LEAVE_STAT
#undef LEAVE_STAT
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

