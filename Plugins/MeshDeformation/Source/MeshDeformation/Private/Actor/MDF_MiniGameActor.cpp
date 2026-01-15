// Gihyeon's MeshDeformation Project
// File: Source/InventoryProject/Actor/MDF_MiniGameActor.cpp

#include "Actor/MDF_MiniGameActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/MDF_MiniGameComponent.h"

AMDF_MiniGameActor::AMDF_MiniGameActor()
{
    // 벽을 움직이게 하려면 Tick이 필요할 수 있으므로 true로 변경
    PrimaryActorTick.bCanEverTick = true;

    // 1. 다이나믹 메시 컴포넌트 생성
    DynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshComponent"));
    SetRootComponent(DynamicMeshComponent);

    if (DynamicMeshComponent)
    {
       // -----------------------------------------------------------------------
       // [필수 1] "이 물체는 움직입니다"라고 선언 (투명화 방지 핵심)
       // -----------------------------------------------------------------------
       DynamicMeshComponent->SetMobility(EComponentMobility::Movable);

       // [기존 설정]
       DynamicMeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
       DynamicMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
       DynamicMeshComponent->SetComplexAsSimpleCollisionEnabled(true, true);
       DynamicMeshComponent->SetSimulatePhysics(false);
       
       // [비동기 쿠킹] 움직일 때 깜빡거리면 false로 꺼보세요.
       DynamicMeshComponent->bUseAsyncCooking = false;

       // -----------------------------------------------------------------------
       // [필수 2] 렌더링/그림자 안전 장치
       // -----------------------------------------------------------------------
       DynamicMeshComponent->bCastDynamicShadow = true; // 동적 그림자 켬
       DynamicMeshComponent->bCastStaticShadow = false; // 스태틱 그림자 끔
       
       // [수정] 에러 나는 bCastRayTracedShadows 코드는 삭제했습니다.
       // 대신 아래 설정을 추가하여 루멘/RT 계산에서 제외시킵니다.
       DynamicMeshComponent->SetAffectDistanceFieldLighting(false); 

       // [필수 3] 바운드 스케일 강제 확장
       // 엔진이 물체가 작다고 착각해서 안 그리는(Culling) 현상을 방지합니다.
       DynamicMeshComponent->SetBoundsScale(10.0f);
    }

    // 2. MiniGameComponent 생성
    MiniGameComponent = CreateDefaultSubobject<UMDF_MiniGameComponent>(TEXT("MiniGameComponent"));

    // 3. 네트워크 설정
    bReplicates = true;
    SetReplicateMovement(true);
}

void AMDF_MiniGameActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 에디터 프리뷰 기능
    if (MiniGameComponent)
    {
       MiniGameComponent->InitializeDynamicMesh();
    }

    if (DynamicMeshComponent)
    {
       // 생성될 때 머티리얼이 비어있으면 투명해 보이므로, 안전장치로 업데이트
       DynamicMeshComponent->UpdateCollision(true);
       DynamicMeshComponent->NotifyMeshUpdated();
    }
}

void AMDF_MiniGameActor::BeginPlay()
{
    Super::BeginPlay();
    
    // [확인용] 시작하자마자 확실하게 Movable인지 다시 한번 강제
    if (DynamicMeshComponent)
    {
        DynamicMeshComponent->SetMobility(EComponentMobility::Movable);
    }
}