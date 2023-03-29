#include "ManagerSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

#include "ManagedActors.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


TAutoConsoleVariable<float> CVarManagerDetectRadius(
    TEXT("manager.radius"),
    1000.f,
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

    for(TWeakObjectPtr<ADynamicActor>& Actor : Dynamics){
        if(!Actor.IsValid())
            continue;

        Actor->Update(DeltaSeconds);
    }
    if(CVarManagerUseBVH.GetValueOnGameThread()){
        for(TWeakObjectPtr<ADynamicActor>& Actor : Dynamics){
            if(!Actor.IsValid())
                continue;

            Tree.MoveProxy(Actor->GetID(), Actor->GetWorldBound());
        }
    }

    if(bHaValidViewLocation){
        const float DetectRadius = CVarManagerDetectRadius.GetValueOnGameThread();
        
        if(CVarManagerUseBVH.GetValueOnGameThread()){
            decltype(ViewLocation)* ViewLocationPtr = &ViewLocation;
            
            Tree.Query(
                [DetectRadius, ViewLocationPtr](const TBVHBound<double>& CurBound){
                    return IntersectSphereAndAABB(
                        VectorLoadAligned(ViewLocationPtr),
                        VectorSetDouble1(DetectRadius),
                        VectorLoadAligned(&CurBound.Lower),
                        VectorLoadAligned(&CurBound.Upper)
                        );
                },
                [](int32, TWeakObjectPtr<AManagedActor>& Actor){
                    if(!Actor.IsValid())
                        return;

                    Actor->SetFlag(1);
                }
                ); 
        }
        else{
            for(TWeakObjectPtr<AManagedActor>& Actor : Whole){
                if(!Actor.IsValid())
                    continue;

                const auto& CurBound = Actor->GetWorldBound();
                if(IntersectSphereAndAABB(
                    VectorLoadAligned(&ViewLocation),
                    VectorSetDouble1(DetectRadius),
                    VectorLoadAligned(&CurBound.Lower),
                    VectorLoadAligned(&CurBound.Upper)
                ))
                    continue;

                Actor->SetFlag(1);
            }
        }
    }

    if(CVarManagerUseBVH.GetValueOnGameThread()){
        for(auto& Wrap : Tree){
            auto& Actor = Wrap.Get<1>();
            if(!Actor.IsValid())
                continue;

            Actor->ChangeColour();
        }
    }
    else{
        for(auto& Actor : Whole){
            if(!Actor.IsValid())
                continue;

            Actor->ChangeColour();
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

