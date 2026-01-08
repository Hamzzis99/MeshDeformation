// Gihyeon's Deformation Project (Helluna)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MDF_DeformableComponent.generated.h"

class UDynamicMeshComponent;
class UNiagaraSystem;
class USoundBase;

/** * [Step 6 최적화] 타격 데이터를 임시 저장하기 위한 구조체 
 * 루프 횟수를 줄이기 위해 여러 타격 지점을 이 구조체에 담아 한 번에 처리합니다.
 */
USTRUCT(BlueprintType)
struct FMDFHitData
{
    GENERATED_BODY()

    UPROPERTY()
    FVector LocalLocation;

    UPROPERTY()
    FVector LocalDirection;

    UPROPERTY()
    float Damage;

    FMDFHitData() : LocalLocation(FVector::ZeroVector), LocalDirection(FVector::ZeroVector), Damage(0.0f) {}
    FMDFHitData(FVector InLoc, FVector InDir, float InDamage) 
        : LocalLocation(InLoc), LocalDirection(InDir), Damage(InDamage) {}
};

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MESHDEFORMATION_API UMDF_DeformableComponent : public UActorComponent
{
    GENERATED_BODY()

public: 
    UMDF_DeformableComponent();

protected:
    virtual void BeginPlay() override;

    /** [MeshDeformation] 포인트 데미지 수신 및 변형 데이터를 큐에 쌓음 */
    UFUNCTION()
    virtual void HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser);

    /** [Step 6 최적화] 모인 타격 지점들을 한 프레임의 끝에서 한 번에 연산 */
    void ProcessDeformationBatch();

    /** [MeshDeformation] 실제 버텍스 변형 연산 (배칭 로직 내부에서 호출됨) */
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

    /** [Step 6 최적화] 타격 데이터를 모으는 시간 (초) 
     * 0이면 다음 프레임에 즉시 처리, 10이면 10초 동안 모았다가 한 번에 처리합니다. 
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "배칭 처리 대기 시간"))
    float BatchProcessDelay = 0.0f;
    
    /** [MeshDeformation|Effect] 변형 시 발생할 나이아가라 파편 시스템 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "파편 이펙트(Niagara)"))
    TObjectPtr<UNiagaraSystem> DebrisSystem;

    /** [MeshDeformation|Effect] 피격 시 재생될 3D 사운드 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "피격 사운드(3D)"))
    TObjectPtr<USoundBase> ImpactSound;
    
    /** [MeshDeformation|Effect] 3D 사운드 거리 감쇄 설정 (여기서 거리에 따른 소리 크기 제어) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "사운드 감쇄 설정(3D Attenuation)"))
    TObjectPtr<USoundAttenuation> ImpactAttenuation;
    
private:
    /** [Step 6] 1프레임 동안 쌓인 타격 지점 리스트 (배칭 큐) */
    TArray<FMDFHitData> HitQueue;

    /** 타이머 핸들 (중복 호출 방지용) */
    FTimerHandle BatchTimerHandle;
};