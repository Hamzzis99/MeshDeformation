// Gihyeon's Inventory Project (Helluna)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h" // 데미지 타입 사용을 위해 추가
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

    /** 데미지 수치 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "발사체 설정|전투", meta = (DisplayName = "데미지 수치"))
    float DamageValue = 20.0f;

    /** [MeshDeformation] 데미지 타입 (예: 원거리, 근거리 등) 
     * 에디터에서 BP_DT_Ranged를 할당하면 됩니다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "발사체 설정|전투", meta = (DisplayName = "데미지 타입 클래스"))
    TSubclassOf<UDamageType> ProjectileDamageType;

    /** 충돌 시 실행될 함수 */
    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};