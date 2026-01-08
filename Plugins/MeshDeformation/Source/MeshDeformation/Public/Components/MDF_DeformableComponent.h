#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MDF_DeformableComponent.generated.h"

/**
 * [MeshDeformation] 메쉬 변형을 담당하는 컴포넌트입니다.
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MESHDEFORMATION_API UMDF_DeformableComponent : public UActorComponent
{
	GENERATED_BODY()

public: 
	UMDF_DeformableComponent();

protected:
	virtual void BeginPlay() override;

	/** [MeshDeformation] 포인트 데미지 수신 및 좌표 변환 처리 */
	UFUNCTION()
	virtual void HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser);

public:
	/** [MeshDeformation] 월드 좌표를 메쉬 로컬 좌표로 변환하는 헬퍼 함수 */
	UFUNCTION(BlueprintCallable, Category = "MeshDeformation|수학")
	FVector ConvertWorldToLocal(FVector WorldLocation);

	/** 시스템 활성화 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "시스템 활성화"))
	bool bIsDeformationEnabled = true;

	/** 디버그용 구체 그리기 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|디버그", meta = (DisplayName = "디버그 포인트 표시"))
	bool bShowDebugPoints = true;
};