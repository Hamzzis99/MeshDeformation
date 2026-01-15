// Gihyeon's MeshDeformation Project

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MDF_MiniGameActor.generated.h"

class UDynamicMeshComponent;
class UMDF_MiniGameComponent;

/**
 * [미니게임 전용 벽 액터]
 * - 독립적인 클래스로 구현 (AMDF_Actor 상속 X)
 * - DynamicMeshComponent + MDF_MiniGameComponent 조합
 */
UCLASS()
class MESHDEFORMATION_API AMDF_MiniGameActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AMDF_MiniGameActor();
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

public:
	// 루트 컴포넌트 (모양 담당)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MDF", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent;

	// 로직 컴포넌트 (미니게임 담당 - 마킹/절단)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MDF", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMDF_MiniGameComponent> MiniGameComponent;
};