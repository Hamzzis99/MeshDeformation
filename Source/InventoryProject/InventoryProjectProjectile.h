#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryProjectProjectile.generated.h"

UCLASS()
class INVENTORYPROJECT_API AInventoryProjectProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	AInventoryProjectProjectile();

	/** 충돌 감지용 구체 */
	UPROPERTY(VisibleDefaultsOnly, Category = "Projectile")
	class USphereComponent* CollisionComponent;

	/** 발사체 이동 컴포넌트 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	class UProjectileMovementComponent* ProjectileMovement;

	/** 외형 메쉬 (블루프린트에서 모델 선택 가능) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	class UStaticMeshComponent* ProjectileMesh;

	/** 데미지 수치 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float DamageValue = 20.0f;

	/** 충돌 시 실행될 함수 */
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};