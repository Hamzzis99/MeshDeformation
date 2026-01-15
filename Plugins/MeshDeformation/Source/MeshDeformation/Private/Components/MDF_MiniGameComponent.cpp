// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Components/MDF_MiniGameComponent.cpp

#include "Components/MDF_MiniGameComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DrawDebugHelpers.h"

// [Geometry Script 헤더] 바운딩 박스 계산용
#include "GeometryScript/MeshQueryFunctions.h" 

UMDF_MiniGameComponent::UMDF_MiniGameComponent()
{
    PrimaryComponentTick.bCanEverTick = true; 
}

void UMDF_MiniGameComponent::StartMarking(FVector WorldLocation)
{
    FVector LocalLoc = ConvertWorldToLocal(WorldLocation);

    // 오차 범위 10.0으로 체크
    if (IsOnBoundary(LocalLoc, 10.0f))
    {
        bIsMarking = true;
        LocalStartPoint = LocalLoc;
        bIsValidCut = false; 
        
        UE_LOG(LogTemp, Log, TEXT("[MiniGame] 마킹 시작 (Valid Start) - 좌표: %s"), *LocalLoc.ToString());
    }
    else
    {
        // 시작은 엄격하게 잡습니다 (시작부터 보정하면 이상하니까)
        UE_LOG(LogTemp, Warning, TEXT("[MiniGame] 실패: 테두리 근처에서 시작하세요. (오차범위 밖)"));
    }
}

void UMDF_MiniGameComponent::UpdateMarking(FVector WorldLocation)
{
    if (!bIsMarking) return;

    FVector LocalEnd = ConvertWorldToLocal(WorldLocation);

    // 1. 박스 미리보기 계산 (기존과 동일)
    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return;
    
    FBox MeshBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    float MinX = FMath::Min(LocalStartPoint.X, LocalEnd.X);
    float MaxX = FMath::Max(LocalStartPoint.X, LocalEnd.X);
    float MinZ = FMath::Min(LocalStartPoint.Z, LocalEnd.Z);
    float MaxZ = FMath::Max(LocalStartPoint.Z, LocalEnd.Z);
    float MinY = MeshBounds.Min.Y - 50.0f;
    float MaxY = MeshBounds.Max.Y + 50.0f;

    CurrentPreviewBox = FBox(FVector(MinX, MinY, MinZ), FVector(MaxX, MaxY, MaxZ));

    // 2. 검증 (단순 조회용)
    // 드래그 중에는 보정 없이 있는 그대로 보여줍니다.
    bIsValidCut = IsOnBoundary(LocalEnd, 10.0f);
}

void UMDF_MiniGameComponent::EndMarking(FVector WorldLocation)
{
    if (!bIsMarking) return;

    // 마지막 위치 가져오기
    FVector FinalLocalPos = ConvertWorldToLocal(WorldLocation);
    
    // -------------------------------------------------------------------------
    // [자동 보정 로직] "스스로 적당히 판단"
    // 마지막 지점이 테두리가 아니라면, 가장 가까운 테두리로 강제 이동(Snap) 시킵니다.
    // -------------------------------------------------------------------------
    if (!IsOnBoundary(FinalLocalPos, 10.0f))
    {
        FVector SnappedPos = SnapToClosestBoundary(FinalLocalPos);
        
        // 보정된 위치와 원래 위치 거리가 너무 멀면(예: 벽 중앙에서 멈춤) 인정 안 함.
        // 하지만 "빗나간 경우"를 위해 50cm 정도까지는 봐줍니다.
        float SnapDist = FVector::Dist(FinalLocalPos, SnappedPos);
        
        if (SnapDist < 50.0f) 
        {
            UE_LOG(LogTemp, Log, TEXT("[MiniGame] 자동 보정 발동! (%.1f 거리만큼 이동해서 테두리에 붙임)"), SnapDist);
            FinalLocalPos = SnappedPos; // 위치 덮어쓰기
            
            // 박스 크기도 보정된 위치로 다시 계산해줘야 함
            UpdateMarking(GetOwner()->GetActorTransform().TransformPosition(FinalLocalPos));
        }
    }

    // 다시 검사 (보정된 위치로)
    bool bFinalCheck = IsOnBoundary(FinalLocalPos, 10.0f); // 10.0 오차

    if (bFinalCheck)
    {
        // [성공] 데이터 저장
        FWeakSpotData NewSpot;
        NewSpot.ID = FGuid::NewGuid();
        NewSpot.LocalBox = CurrentPreviewBox;
        
        NewSpot.MaxHP = CalculateHPFromBox(CurrentPreviewBox);
        NewSpot.CurrentHP = NewSpot.MaxHP;
        NewSpot.bIsBroken = false;

        WeakSpots.Add(NewSpot);

        UE_LOG(LogTemp, Display, TEXT("[MiniGame] >> 영역 확정 성공! (초록색) - ID: %s"), *NewSpot.ID.ToString());
    }
    else
    {
        // [실패 시 상세 로그] 왜 안 됐는지 알려줌
        UE_LOG(LogTemp, Warning, TEXT("[MiniGame] >> 취소됨: 끝점이 테두리에 닿지 않았습니다."));
        
        // 디버깅: 현재 좌표와 테두리 정보 출력
        UDynamicMeshComponent* DynComp = GetOwner()->FindComponentByClass<UDynamicMeshComponent>();
        if (DynComp)
        {
            FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());
            UE_LOG(LogTemp, Warning, TEXT("    - 내 좌표: %s"), *FinalLocalPos.ToString());
            UE_LOG(LogTemp, Warning, TEXT("    - 테두리 X: %.1f / %.1f"), Bounds.Min.X, Bounds.Max.X);
            UE_LOG(LogTemp, Warning, TEXT("    - 테두리 Z: %.1f / %.1f"), Bounds.Min.Z, Bounds.Max.Z);
        }
    }

    bIsMarking = false;
}

// -----------------------------------------------------------------------------
// [유틸리티] 테두리 감지
// -----------------------------------------------------------------------------
bool UMDF_MiniGameComponent::IsOnBoundary(FVector LocalLoc, float Tolerance) const
{
    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return false;

    FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    // 거리 절댓값 계산
    float DistMinX = FMath::Abs(LocalLoc.X - Bounds.Min.X);
    float DistMaxX = FMath::Abs(LocalLoc.X - Bounds.Max.X);
    float DistMinZ = FMath::Abs(LocalLoc.Z - Bounds.Min.Z);
    float DistMaxZ = FMath::Abs(LocalLoc.Z - Bounds.Max.Z);

    // X축 테두리나 Z축 테두리 중 하나라도 Tolerance 안에 들어오면 OK
    bool bOnX = (DistMinX <= Tolerance) || (DistMaxX <= Tolerance);
    bool bOnZ = (DistMinZ <= Tolerance) || (DistMaxZ <= Tolerance);

    return bOnX || bOnZ;
}

// [New] 가장 가까운 테두리 좌표를 찾아서 반환 (자동 보정용)
FVector UMDF_MiniGameComponent::SnapToClosestBoundary(FVector LocalLoc) const
{
    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return LocalLoc;

    FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());
    FVector Snapped = LocalLoc;

    // 각 테두리까지의 거리
    float DistMinX = FMath::Abs(LocalLoc.X - Bounds.Min.X);
    float DistMaxX = FMath::Abs(LocalLoc.X - Bounds.Max.X);
    float DistMinZ = FMath::Abs(LocalLoc.Z - Bounds.Min.Z);
    float DistMaxZ = FMath::Abs(LocalLoc.Z - Bounds.Max.Z);

    // 가장 가까운 거리 찾기
    float MinDist = FMath::Min(DistMinX, FMath::Min(DistMaxX, FMath::Min(DistMinZ, DistMaxZ)));

    // 그 테두리로 좌표 강제 이동
    if (MinDist == DistMinX) Snapped.X = Bounds.Min.X;
    else if (MinDist == DistMaxX) Snapped.X = Bounds.Max.X;
    else if (MinDist == DistMinZ) Snapped.Z = Bounds.Min.Z;
    else if (MinDist == DistMaxZ) Snapped.Z = Bounds.Max.Z;

    return Snapped;
}

float UMDF_MiniGameComponent::CalculateHPFromBox(const FBox& Box) const
{
    FVector Size = Box.GetSize();
    float Area = Size.X * Size.Z;
    return 100.0f + (Area * HPDensityMultiplier);
}

void UMDF_MiniGameComponent::TryBreach(const FHitResult& HitInfo, float DamageAmount)
{
    UE_LOG(LogTemp, Log, TEXT("[MiniGame] 총알이 맞았습니다!"));
}

void UMDF_MiniGameComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!GetOwner()) return;
    FTransform ActorTrans = GetOwner()->GetActorTransform();

    // 1. 저장된 약점 (Cyan)
    for (const FWeakSpotData& Spot : WeakSpots)
    {
        if (Spot.bIsBroken) continue;
        DrawDebugBox(GetWorld(), ActorTrans.TransformPosition(Spot.LocalBox.GetCenter()), Spot.LocalBox.GetExtent(), GetOwner()->GetActorQuat(), FColor::Cyan, false, -1.0f, 0, 1.5f);
    }

    // 2. 드래그 미리보기 (Green/Red)
    if (bIsMarking)
    {
        FColor DrawColor = bIsValidCut ? FColor::Green : FColor::Red;
        DrawDebugBox(GetWorld(), ActorTrans.TransformPosition(CurrentPreviewBox.GetCenter()), CurrentPreviewBox.GetExtent(), GetOwner()->GetActorQuat(), DrawColor, false, -1.0f, 0, 3.0f);
    }
}