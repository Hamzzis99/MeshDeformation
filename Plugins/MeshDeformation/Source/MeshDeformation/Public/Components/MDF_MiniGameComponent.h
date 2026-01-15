// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Components/MDF_MiniGameComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/MDF_DeformableComponent.h"
#include "MDF_MiniGameComponent.generated.h"

/**
 * [Step 4] 약점 데이터 구조체
 * - 리더가 설계하고 슈터가 파괴할 영역 정보
 */
USTRUCT(BlueprintType)
struct FWeakSpotData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid ID;

    UPROPERTY(BlueprintReadOnly)
    FBox LocalBox;

    UPROPERTY(BlueprintReadOnly)
    float CurrentHP;

    UPROPERTY(BlueprintReadOnly)
    float MaxHP;

    UPROPERTY(BlueprintReadOnly)
    bool bIsBroken = false;

    FWeakSpotData() 
        : ID(FGuid::NewGuid()), LocalBox(FBox(EForceInit::ForceInit)), CurrentHP(100.f), MaxHP(100.f), bIsBroken(false) {}
};

/**
 * [MDF_MiniGameComponent]
 * - 리더의 절단 설계(Marking)와 슈터의 파괴(Breach)를 담당하는 핵심 컴포넌트
 * - 부모 클래스(Deformable)의 변형 로직을 제어하는 문지기 역할을 수행함
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MESHDEFORMATION_API UMDF_MiniGameComponent : public UMDF_DeformableComponent
{
    GENERATED_BODY()

public:
    UMDF_MiniGameComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    // -------------------------------------------------------------------------
    // [Step 1~3] 리더(Leader) 기능: 설계(Marking)
    // -------------------------------------------------------------------------
    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void StartMarking(FVector WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void UpdateMarking(FVector WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void EndMarking(FVector WorldLocation);

    // -------------------------------------------------------------------------
    // [Step 4~5] 슈터(Shooter) 기능: 파괴(Breach)
    // -------------------------------------------------------------------------

    /** * 약점 타격 시도 (문지기에게 보고서 제출)
     * @return true면 약점 명중, false면 빗나감
     */
    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    bool TryBreach(const FHitResult& HitInfo, float DamageAmount);

protected:
    // -------------------------------------------------------------------------
    // [Step 5 핵심] 부모의 데미지 처리 함수 가로채기 (Gatekeeper)
    // -------------------------------------------------------------------------
    virtual void HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser) override;

protected:
    // [유틸리티 함수]
    bool IsOnBoundary(FVector LocalLoc, float Tolerance = 10.0f) const;
    float CalculateHPFromBox(const FBox& Box) const;
    FVector SnapToClosestBoundary(FVector LocalLoc) const;
    FVector GetLocalLocationFromWorld(FVector WorldLoc) const;
    void ExecuteDestruction(int32 WeakSpotIndex);

protected:
    // [마킹 상태 변수]
    bool bIsMarking = false;     
    bool bIsValidCut = false;    
    bool bHasFirstPoint = false; 

    FVector LocalStartPoint;          
    FVector LocalFirstBoundaryPoint;  
    FBox CurrentPreviewBox;           

    // [데이터 저장소]
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MDF|MiniGame", meta = (DisplayName = "약점 데이터 목록"))
    TArray<FWeakSpotData> WeakSpots;

    // [설정]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MDF|Config", meta = (DisplayName = "HP 밀도 계수 (내구도)"))
    float HPDensityMultiplier = 0.1f; 
};