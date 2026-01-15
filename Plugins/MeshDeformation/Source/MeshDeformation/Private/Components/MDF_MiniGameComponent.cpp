// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Components/MDF_MiniGameComponent.cpp

#include "Components/MDF_MiniGameComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DrawDebugHelpers.h"
#include "GeometryScript/MeshQueryFunctions.h" 

UMDF_MiniGameComponent::UMDF_MiniGameComponent()
{
    PrimaryComponentTick.bCanEverTick = true; 
}

// [좌표 변환] 액터 기준이 아니라 '다이나믹 메시 컴포넌트' 기준 (정확도 핵심)
FVector UMDF_MiniGameComponent::GetLocalLocationFromWorld(FVector WorldLoc) const
{
    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (DynComp)
    {
        return DynComp->GetComponentTransform().InverseTransformPosition(WorldLoc);
    }
    return GetOwner() ? GetOwner()->GetActorTransform().InverseTransformPosition(WorldLoc) : WorldLoc;
}

void UMDF_MiniGameComponent::StartMarking(FVector WorldLocation)
{
    // 일단 무조건 시작 (빨간색으로 보일 예정)
    bIsMarking = true;
    bIsValidCut = false; 
    LocalStartPoint = GetLocalLocationFromWorld(WorldLocation);

    // 로그 출력
    UE_LOG(LogTemp, Log, TEXT("--- [MiniGame] 드래그 시작 ---"));
}

void UMDF_MiniGameComponent::UpdateMarking(FVector WorldLocation)
{
    if (!bIsMarking) return;

    FVector LocalEnd = GetLocalLocationFromWorld(WorldLocation);

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return;
    
    FBox MeshBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    // -------------------------------------------------------------------------
    // [알고리즘] 스마트 축 감지 (Smart Axis Detection)
    // 리더가 앞면(X)을 쏘는지, 옆면(Y)을 쏘는지 판단해서 박스 모양을 결정
    // -------------------------------------------------------------------------
    
    float DistMinX = FMath::Abs(LocalStartPoint.X - MeshBounds.Min.X);
    float DistMaxX = FMath::Abs(LocalStartPoint.X - MeshBounds.Max.X);
    
    // 시작점이 X축(앞/뒤) 테두리에 가까운가? (혹은 X좌표가 테두리 근처인가)
    // 단순히 거리만 보는게 아니라, 내가 어느 면을 보고 쏘는지 알아야 함.
    // 여기서는 "두 점의 X값 차이"가 "두 점의 Y값 차이"보다 작으면 옆면(Y)을 긋는다고 가정하거나
    // 시작점의 위치로 판단함.
    
    bool bIsFacingX = (DistMinX < 20.0f || DistMaxX < 20.0f);

    FVector MinVec, MaxVec;

    if (bIsFacingX) 
    {
        // [앞면/뒷면 그리기] -> 두께는 X축, 그리는 평면은 YZ
        MinVec.X = MeshBounds.Min.X - 50.0f; // 두께 확장
        MaxVec.X = MeshBounds.Max.X + 50.0f;
        
        MinVec.Y = FMath::Min(LocalStartPoint.Y, LocalEnd.Y);
        MaxVec.Y = FMath::Max(LocalStartPoint.Y, LocalEnd.Y);
        MinVec.Z = FMath::Min(LocalStartPoint.Z, LocalEnd.Z);
        MaxVec.Z = FMath::Max(LocalStartPoint.Z, LocalEnd.Z);
    }
    else 
    {
        // [옆면 그리기] -> 두께는 Y축, 그리는 평면은 XZ
        MinVec.Y = MeshBounds.Min.Y - 50.0f; // 두께 확장
        MaxVec.Y = MeshBounds.Max.Y + 50.0f;

        MinVec.X = FMath::Min(LocalStartPoint.X, LocalEnd.X);
        MaxVec.X = FMath::Max(LocalStartPoint.X, LocalEnd.X);
        MinVec.Z = FMath::Min(LocalStartPoint.Z, LocalEnd.Z);
        MaxVec.Z = FMath::Max(LocalStartPoint.Z, LocalEnd.Z);
    }

    CurrentPreviewBox = FBox(MinVec, MaxVec);

    // -------------------------------------------------------------------------
    // [실시간 검증] 빨강 vs 초록 결정
    // -------------------------------------------------------------------------
    bool bStartOK = IsOnBoundary(LocalStartPoint, 15.0f);
    bool bEndOK = IsOnBoundary(LocalEnd, 15.0f);

    if (bStartOK && bEndOK)
    {
        bIsValidCut = true; // 둘 다 테두리 -> 초록색
    }
    else
    {
        bIsValidCut = false; // 하나라도 아님 -> 빨간색
    }
}

void UMDF_MiniGameComponent::EndMarking(FVector WorldLocation)
{
    if (!bIsMarking) return;

    // 마지막으로 한 번 더 업데이트 (정확한 위치 반영)
    UpdateMarking(WorldLocation);

    if (bIsValidCut)
    {
        // [성공] 초록색 상태에서 놓았음 -> 저장
        FWeakSpotData NewSpot;
        NewSpot.ID = FGuid::NewGuid();
        NewSpot.LocalBox = CurrentPreviewBox;
        NewSpot.MaxHP = CalculateHPFromBox(CurrentPreviewBox);
        NewSpot.CurrentHP = NewSpot.MaxHP;
        NewSpot.bIsBroken = false;

        WeakSpots.Add(NewSpot);

        UE_LOG(LogTemp, Display, TEXT("[MiniGame] 저장 완료! (ID: %s, HP: %.1f)"), *NewSpot.ID.ToString(), NewSpot.MaxHP);
    }
    else
    {
        // [실패] 빨간색 상태에서 놓았음 -> 초기화
        UE_LOG(LogTemp, Warning, TEXT("[MiniGame] 취소됨 (조건 불만족)"));
    }

    bIsMarking = false;
}

// -----------------------------------------------------------------------------
// [유틸리티] 테두리 감지 (X, Y, Z 모든 면 체크)
// -----------------------------------------------------------------------------
bool UMDF_MiniGameComponent::IsOnBoundary(FVector LocalLoc, float Tolerance) const
{
    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return false;

    FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    // 1. X축 끝 (앞/뒤 면)
    bool bOnX = FMath::IsNearlyEqual(LocalLoc.X, Bounds.Min.X, Tolerance) || 
                FMath::IsNearlyEqual(LocalLoc.X, Bounds.Max.X, Tolerance);
    
    // 2. Y축 끝 (좌/우 면)
    bool bOnY = FMath::IsNearlyEqual(LocalLoc.Y, Bounds.Min.Y, Tolerance) || 
                FMath::IsNearlyEqual(LocalLoc.Y, Bounds.Max.Y, Tolerance);

    // 3. Z축 끝 (바닥/천장)
    bool bOnZ = FMath::IsNearlyEqual(LocalLoc.Z, Bounds.Min.Z, Tolerance) || 
                FMath::IsNearlyEqual(LocalLoc.Z, Bounds.Max.Z, Tolerance);

    return bOnX || bOnY || bOnZ;
}

float UMDF_MiniGameComponent::CalculateHPFromBox(const FBox& Box) const
{
    FVector Size = Box.GetSize();
    // 부피를 쓰면 두께가 100이라 너무 커짐. 면적 근사치로 계산
    float Volume = Size.X * Size.Y * Size.Z;
    return 100.0f + (Volume * HPDensityMultiplier * 0.01f); // 계수 조정
}

void UMDF_MiniGameComponent::TryBreach(const FHitResult& HitInfo, float DamageAmount)
{
    UE_LOG(LogTemp, Log, TEXT("[MiniGame] 총알이 맞았습니다!"));
}

void UMDF_MiniGameComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp) return;

    // [중요] 디버그 박스 그릴 때도 "컴포넌트 트랜스폼" 사용
    FTransform CompTrans = DynComp->GetComponentTransform();

    // 1. 저장된 약점 (Cyan - 청록색)
    for (const FWeakSpotData& Spot : WeakSpots)
    {
        if (Spot.bIsBroken) continue;
        DrawDebugBox(GetWorld(), CompTrans.TransformPosition(Spot.LocalBox.GetCenter()), Spot.LocalBox.GetExtent(), CompTrans.GetRotation(), FColor::Cyan, false, -1.0f, 0, 1.5f);
    }

    // 2. 드래그 중 박스 (Green=성공, Red=실패)
    if (bIsMarking)
    {
        FColor DrawColor = bIsValidCut ? FColor::Green : FColor::Red;
        DrawDebugBox(GetWorld(), CompTrans.TransformPosition(CurrentPreviewBox.GetCenter()), CurrentPreviewBox.GetExtent(), CompTrans.GetRotation(), DrawColor, false, -1.0f, 0, 3.0f);
    }
}