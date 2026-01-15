// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Components/MDF_MiniGameComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/MDF_DeformableComponent.h" // 부모 클래스 포함
#include "MDF_MiniGameComponent.generated.h"

/**
 * [Step 4] 약점 데이터 구조체
 * - 리더가 생성한 절단 영역 정보와 체력 상태를 저장합니다.
 */
USTRUCT(BlueprintType)
struct FWeakSpotData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid ID; // 고유 식별자

    UPROPERTY(BlueprintReadOnly)
    FBox LocalBox; // 절단 영역 (로컬 좌표)

    UPROPERTY(BlueprintReadOnly)
    float CurrentHP;

    UPROPERTY(BlueprintReadOnly)
    float MaxHP;

    UPROPERTY(BlueprintReadOnly)
    bool bIsBroken = false; // 파괴 여부

    // 기본 생성자
    FWeakSpotData()
        : ID(FGuid::NewGuid()), LocalBox(FBox(EForceInit::ForceInit)), CurrentHP(100.f), MaxHP(100.f), bIsBroken(false) {}
};

/**
 * [Step 5] 미니게임용 변형 컴포넌트
 * - 기능 1: 마킹 시스템 (Start -> Update -> End) : 엣지-투-엣지 알고리즘 적용
 * - 기능 2: 약점 데이터 관리 (Save Data & HP) : [Step 4 추가됨]
 * - 기능 3: TryBreach (총 맞으면 약점 확인 후 파괴)
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MESHDEFORMATION_API UMDF_MiniGameComponent : public UMDF_DeformableComponent
{
    GENERATED_BODY()

public:
    UMDF_MiniGameComponent();

    // 매 프레임 디버그 박스를 그리기 위해 Tick 사용
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    // -------------------------------------------------------------------------
    // [리더용] 마킹 시스템 (드래그 방식)
    // -------------------------------------------------------------------------
    
    // 마우스 클릭 시: 시작점이 테두리인지 확인
    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void StartMarking(FVector WorldLocation);

    // 마우스 드래그 중: 박스 크기 계산 및 시각화
    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void UpdateMarking(FVector WorldLocation);

    // 마우스 뗌: 유효성 검증 후 데이터 확정 및 저장
    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void EndMarking(FVector WorldLocation);

    // -------------------------------------------------------------------------
    // [슈터용] 벽을 뚫으려는 시도 (검정색이면 팅겨냄)
    // -------------------------------------------------------------------------
    UFUNCTION(BlueprintCallable, Category = "MDF|MiniGame")
    void TryBreach(const FHitResult& HitInfo, float DamageAmount);

protected:
    // 내부 로직: 현재 로컬 좌표가 메쉬의 테두리(Edge) 근처인지 판별
    bool IsOnBoundary(FVector LocalLoc, float Tolerance = 10.0f) const;

    // [Step 4 New] 박스 크기에 따른 HP 계산
    float CalculateHPFromBox(const FBox& Box) const;
    
    FVector SnapToClosestBoundary(FVector LocalLoc) const;

protected:
    // -------------------------------------------------------------------------
    // [상태 변수]
    // -------------------------------------------------------------------------
    bool bIsMarking = false;     // 현재 드래그 중인가?
    bool bIsValidCut = false;    // 현재 드래그가 유효한(연결된) 상태인가?
    
    FVector LocalStartPoint;     // 드래그 시작 지점 (로컬)
    FBox CurrentPreviewBox;      // 현재 계산된 절단 영역 박스

    // [Step 4 New] 저장된 약점 데이터 리스트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MDF|MiniGame")
    TArray<FWeakSpotData> WeakSpots;

    // [Step 4 New] HP 밀도 계수 (면적 당 HP)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MDF|Config")
    float HPDensityMultiplier = 0.1f; 
};