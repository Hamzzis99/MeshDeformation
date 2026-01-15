// Gihyeon's MeshDeformation Project
#include "Actor/MDF_MiniGameActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/MDF_MiniGameComponent.h"
#include "Components/SceneComponent.h"

AMDF_MiniGameActor::AMDF_MiniGameActor()
{
    PrimaryActorTick.bCanEverTick = true;

    DefaultSceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
    SetRootComponent(DefaultSceneRoot);

    DynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshComponent"));
    if (DynamicMeshComponent)
    {
       DynamicMeshComponent->SetupAttachment(DefaultSceneRoot); 
       DynamicMeshComponent->SetMobility(EComponentMobility::Movable);

       // 1. 충돌 설정 (물리 끔)
       DynamicMeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
       DynamicMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
       DynamicMeshComponent->SetComplexAsSimpleCollisionEnabled(true, true);
       DynamicMeshComponent->SetSimulatePhysics(false);
       
       // -----------------------------------------------------------------------
       // [★최종 수정] 렌더링 증발/잔상 해결을 위한 모든 옵션 차단
       // -----------------------------------------------------------------------
       
       // [Culling 방지] 바운드를 강제로 매우 크게 설정 (화면 밖 처리 방지)
       DynamicMeshComponent->SetBoundsScale(1000.0f); 
       
       // [Lumen/RayTracing 방지] 동적 메쉬는 무거운 연산에서 제외해야 잔상이 안 남음
       DynamicMeshComponent->bCastDynamicShadow = true;
       DynamicMeshComponent->bCastStaticShadow = false;
       DynamicMeshComponent->SetAffectDistanceFieldLighting(false); // 루멘 거리장 끄기
       DynamicMeshComponent->bAffectDynamicIndirectLighting = false; // 간접광(GI) 영향 끄기
       DynamicMeshComponent->bAffectDistanceFieldLighting = false;

       // [NavMesh 방지] 이동 시 프레임 드랍 및 렌더링 렉 유발 원인 차단
       DynamicMeshComponent->SetCanEverAffectNavigation(false);

       // [Cooking] 비동기 쿠킹 끄기 (위치 동기화 우선)
       DynamicMeshComponent->bUseAsyncCooking = false;
       
       // [Overlay] 복잡한 오버랩 이벤트도 끄기 (필요하다면 켜세요)
       DynamicMeshComponent->SetGenerateOverlapEvents(false);
    }

    MiniGameComponent = CreateDefaultSubobject<UMDF_MiniGameComponent>(TEXT("MiniGameComponent"));

    bReplicates = true;
    SetReplicateMovement(true);
}

void AMDF_MiniGameActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (MiniGameComponent)
    {
       MiniGameComponent->InitializeDynamicMesh();
    }

    if (DynamicMeshComponent)
    {
       DynamicMeshComponent->UpdateCollision(true);
       DynamicMeshComponent->NotifyMeshUpdated();
    }
}

void AMDF_MiniGameActor::BeginPlay()
{
    Super::BeginPlay();
    
    // 시작 시 확실하게 Movable 재확인
    if (DynamicMeshComponent)
    {
        DynamicMeshComponent->SetMobility(EComponentMobility::Movable);
    }
}