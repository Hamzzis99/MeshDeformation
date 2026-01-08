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
    UPROPERTY(VisibleDefaultsOnly, Category = "발사체 설정")
    class USphereComponent* CollisionComponent;

    /** 발사체 이동 컴포넌트 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "발사체 설정")
    class UProjectileMovementComponent* ProjectileMovement;

    /** 외형 메쉬 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "발사체 설정")
    class UStaticMeshComponent* ProjectileMesh;

    /** 데미지 수치 (블루프린트에서 한글로 표시됨) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "발사체 설정|전투", meta = (DisplayName = "데미지 수치"))
    float DamageValue = 20.0f;

    /** 충돌 시 실행될 함수 */
    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};