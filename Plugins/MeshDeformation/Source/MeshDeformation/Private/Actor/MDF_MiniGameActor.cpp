// Gihyeon's MeshDeformation Project

#include "Actor/MDF_MiniGameActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/MDF_MiniGameComponent.h"

AMDF_MiniGameActor::AMDF_MiniGameActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. 다이나믹 메시 컴포넌트 생성 및 루트 설정 (기존과 동일)
	DynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshComponent"));
	SetRootComponent(DynamicMeshComponent);

	if (DynamicMeshComponent)
	{
		// [AMDF_Actor의 설정 그대로 복사]
		DynamicMeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
		DynamicMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		DynamicMeshComponent->SetComplexAsSimpleCollisionEnabled(true, true);
		DynamicMeshComponent->SetSimulatePhysics(false);
		DynamicMeshComponent->bUseAsyncCooking = true;
	}

	// 2. [핵심 변경] 일반 Deformable 대신 'MiniGameComponent' 생성!
	MiniGameComponent = CreateDefaultSubobject<UMDF_MiniGameComponent>(TEXT("MiniGameComponent"));

	// 3. 네트워크 설정
	bReplicates = true;
	SetReplicateMovement(true);
}

void AMDF_MiniGameActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터 프리뷰 기능 (기존과 동일)
	if (MiniGameComponent)
	{
		// 부모 클래스(MDF_DeformableComponent)에 있는 함수 호출
		MiniGameComponent->InitializeDynamicMesh();
	}

	if (DynamicMeshComponent)
	{
		DynamicMeshComponent->UpdateCollision(true);
	}
}

void AMDF_MiniGameActor::BeginPlay()
{
	Super::BeginPlay();
}