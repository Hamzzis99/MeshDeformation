#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MDF_DeformableComponent.generated.h"

class UDynamicMeshComponent;

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MESHDEFORMATION_API UMDF_DeformableComponent : public UActorComponent
{
    GENERATED_BODY()

public: 
    UMDF_DeformableComponent();

protected:
    virtual void BeginPlay() override;

    /** [MeshDeformation] 포인트 데미지 수신 및 변형 처리 */
    UFUNCTION()
    virtual void HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser);

    /** [MeshDeformation] 실제 버텍스 변형 연산 */
    void DeformMesh(UDynamicMeshComponent* MeshComp, const FVector& LocalLocation, const FVector& LocalDirection, float Damage);

public:
    /** 원본으로 사용할 StaticMesh 에셋 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "스태틱 메쉬(StaticMesh)"))
    TObjectPtr<UStaticMesh> SourceStaticMesh;
    
    /** 에셋을 기반으로 DynamicMesh를 초기화하는 함수 */
    UFUNCTION(BlueprintCallable, Category = "MeshDeformation")
    void InitializeDynamicMesh();
    
    /** 월드 좌표 -> 로컬 좌표 변환 */
    UFUNCTION(BlueprintCallable, Category = "MeshDeformation|수학")
    FVector ConvertWorldToLocal(FVector WorldLocation);

    /** 월드 방향 -> 로컬 방향 변환 */
    UFUNCTION(BlueprintCallable, Category = "MeshDeformation|수학")
    FVector ConvertWorldDirectionToLocal(FVector WorldDirection);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "시스템 활성화"))
    bool bIsDeformationEnabled = true;

    /** 타격 지점 주변의 변형 반경 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "변형 반경"))
    float DeformRadius = 30.0f;

    /** 타격 시 안으로 밀려 들어가는 강도 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "변형 강도"))
    float DeformStrength = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|디버그", meta = (DisplayName = "디버그 포인트 표시"))
    bool bShowDebugPoints = true;
};