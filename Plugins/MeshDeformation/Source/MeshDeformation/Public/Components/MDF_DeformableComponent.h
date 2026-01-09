// Gihyeon's Deformation Project (Helluna)

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MDF_DeformableComponent.generated.h"

class UDynamicMeshComponent;
class UNiagaraSystem;
class USoundBase;

/** * [Step 6 최적화 -> Step 7-1 네트워크 확장] 
 * 타격 데이터를 임시 저장 및 네트워크 전송하기 위한 구조체 
 * TArray<FMDFHitData> 형태로 RPC 인자로 전달됩니다.
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

    UPROPERTY()
    TSubclassOf<UDamageType> DamageTypeClass;

    FMDFHitData() : LocalLocation(FVector::ZeroVector), LocalDirection(FVector::ForwardVector), Damage(0.f), DamageTypeClass(nullptr) {}
    FMDFHitData(FVector Loc, FVector Dir, float Dmg, TSubclassOf<UDamageType> DmgType) 
        : LocalLocation(Loc), LocalDirection(Dir), Damage(Dmg), DamageTypeClass(DmgType) {}
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

    /** * [Step 6 최적화] 모인 타격 지점들을 한 프레임의 끝에서 한 번에 연산 
     * (서버에서만 호출되어 RPC를 발송하는 역할로 변경 예정)
     */
    void ProcessDeformationBatch();

    /**
     * [Step 1 네트워크 추가]
     * 서버에서 배칭된 타격 데이터를 모든 클라이언트에게 전송하여 실행합니다.
     * Reliable: 지형 변형은 게임플레이(엄폐물 제거 등)에 영향을 주므로 패킷 유실을 허용하지 않습니다.
     * NetMulticast: 서버 -> 모든 클라이언트 (서버 자신 포함 가능, 구현부에 따라 처리)
     * * @param BatchedHits - 서버에서 0.1초(BatchProcessDelay)간 모은 타격 데이터 배열
     */
    UFUNCTION(NetMulticast, Reliable)
    void NetMulticast_ApplyDeformation(const TArray<FMDFHitData>& BatchedHits);

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

    /** [Step 6 최적화] 타격 데이터를 모으는 시간 (초) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "배칭 처리 대기 시간"))
    float BatchProcessDelay = 0.0f;
    
    /** [MeshDeformation|Effect] 변형 시 발생할 나이아가라 파편 시스템 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "파편 이펙트(Niagara)"))
    TObjectPtr<UNiagaraSystem> DebrisSystem;

    /** [MeshDeformation|Effect] 피격 시 재생될 3D 사운드 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "피격 사운드(3D)"))
    TObjectPtr<USoundBase> ImpactSound;
    
    /** [MeshDeformation|Effect] 3D 사운드 거리 감쇄 설정 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정", meta = (DisplayName = "사운드 감쇄 설정(3D Attenuation)"))
    TObjectPtr<USoundAttenuation> ImpactAttenuation;
    
    /** [MeshDeformation|설정] 원거리 공격 판정용 클래스 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정")
    TSubclassOf<UDamageType> RangedDamageType;

    /** [MeshDeformation|설정] 근접 공격 판정용 클래스 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MeshDeformation|설정")
    TSubclassOf<UDamageType> MeleeDamageType;
    
private:
    /** [Step 6] 1프레임 동안 쌓인 타격 지점 리스트 (배칭 큐) */
    TArray<FMDFHitData> HitQueue;

    /** 타이머 핸들 (중복 호출 방지용) */
    FTimerHandle BatchTimerHandle;
};