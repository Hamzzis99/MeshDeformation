// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Components/MDF_MiniGameComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/MDF_DeformableComponent.h"
#include "MDF_MiniGameComponent.generated.h"

/**
 * [Step 4] 약점 데이터 구조체
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
 * [Step 5] 미니게임용 변형 컴포넌트
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MESHDEFORMATION_API UMDF_MiniGameComponent : public UMDF_DeformableComponent
{
    GENERATED_BODY()

public:
    UMDF_MiniGameComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    // [리더용] 마킹 시스템
    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void StartMarking(FVector WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void UpdateMarking(FVector WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void EndMarking(FVector WorldLocation);

    // [슈터용] 파괴 시스템
    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void TryBreach(const FHitResult& HitInfo, float DamageAmount);

protected:
    // 내부 로직
    bool IsOnBoundary(FVector LocalLoc, float Tolerance = 10.0f) const;
    float CalculateHPFromBox(const FBox& Box) const;
    FVector GetLocalLocationFromWorld(FVector WorldLoc) const;

    // 파괴 실행 함수 (나중에 구현)
    // void ExecuteDestruction(int32 WeakSpotIndex); 

protected:
    bool bIsMarking = false;     
    bool bIsValidCut = false;    
    
    FVector LocalStartPoint;     
    FBox CurrentPreviewBox;      

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MDF|MiniGame")
    TArray<FWeakSpotData> WeakSpots;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MDF|Config")
    float HPDensityMultiplier = 0.1f; 
};