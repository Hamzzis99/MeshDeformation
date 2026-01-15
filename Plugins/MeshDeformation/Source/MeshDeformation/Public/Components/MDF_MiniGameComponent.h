// Gihyeon's MeshDeformation Project

#pragma once

#include "CoreMinimal.h"
#include "Components/MDF_DeformableComponent.h" // 부모 클래스 포함
#include "MDF_MiniGameComponent.generated.h"

/**
 * [Step 5] 미니게임용 변형 컴포넌트
 * - 기능 1: PaintWeakness (레이저 맞으면 빨개짐)
 * - 기능 2: TryBreach (총 맞으면 빨간 곳인지 확인 후 찌그러짐/구멍)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MESHDEFORMATION_API UMDF_MiniGameComponent : public UMDF_DeformableComponent
{
	GENERATED_BODY()

public:
	UMDF_MiniGameComponent();

public:
	// -------------------------------------------------------------------------
	// [리더용] 벽에 약점을 표시하는 함수 (Vertex Color 변경)
	// -------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
	void PaintWeakness(FVector WorldLocation, float Radius = 50.0f);

	// -------------------------------------------------------------------------
	// [슈터용] 벽을 뚫으려는 시도 (검정색이면 팅겨냄)
	// -------------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
	void TryBreach(const FHitResult& HitInfo, float DamageAmount);

protected:
	// 내부적으로 색칠하는 로직 (Geometry Script 활용)
	void ApplyVertexColorPaint(FVector LocalLocation, float Radius);
};