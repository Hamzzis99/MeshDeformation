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

	/** * [MeshDeformation] 부모 액터로부터 포인트 데미지를 전달받는 함수 
	 * 블루프린트에서 오버라이드 가능하도록 BlueprintAuthorityOnly 등을 고려할 수 있으나,
	 * 우선은 서버 전용 로직으로 안전하게 구현합니다.
	 */
	UFUNCTION()
	virtual void HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser);

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 시스템 활성화 여부 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "시스템 활성화"))
	bool bIsDeformationEnabled = true;
};