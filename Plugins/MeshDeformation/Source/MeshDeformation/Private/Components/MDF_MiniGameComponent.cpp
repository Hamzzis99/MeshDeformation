// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Components/MDF_MiniGameComponent.cpp

#include "Components/MDF_MiniGameComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DrawDebugHelpers.h"
#include "GeometryScript/MeshQueryFunctions.h" 
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"

UMDF_MiniGameComponent::UMDF_MiniGameComponent()
{
    PrimaryComponentTick.bCanEverTick = true; 
}

FVector UMDF_MiniGameComponent::GetLocalLocationFromWorld(FVector WorldLoc) const
{
    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (DynComp) return DynComp->GetComponentTransform().InverseTransformPosition(WorldLoc);
    return GetOwner() ? GetOwner()->GetActorTransform().InverseTransformPosition(WorldLoc) : WorldLoc;
}

void UMDF_MiniGameComponent::StartMarking(FVector WorldLocation)
{
    bIsMarking = true;
    bIsValidCut = false; 
    bHasFirstPoint = false;

    // 중앙에서 클릭해도 됨 (일단 시작점 기록)
    LocalStartPoint = GetLocalLocationFromWorld(WorldLocation);

    // 운 좋게 시작부터 테두리면 바로 등록
    if (IsOnBoundary(LocalStartPoint, 15.0f))
    {
        bHasFirstPoint = true;
        LocalFirstBoundaryPoint = LocalStartPoint;
        UE_LOG(LogTemp, Log, TEXT("[MiniGame] 시작부터 테두리! Point A 고정."));
    }
}

void UMDF_MiniGameComponent::UpdateMarking(FVector WorldLocation)
{
    if (!bIsMarking) return;

    FVector CurrentLocalPos = GetLocalLocationFromWorld(WorldLocation);

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return;
    
    FBox MeshBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    // -------------------------------------------------------------------------
    // [1단계] 첫 번째 테두리(Point A) 찾기 (아직 못 찾았다면)
    // -------------------------------------------------------------------------
    if (!bHasFirstPoint)
    {
        if (IsOnBoundary(CurrentLocalPos, 15.0f))
        {
            bHasFirstPoint = true;
            LocalFirstBoundaryPoint = CurrentLocalPos;
            UE_LOG(LogTemp, Log, TEXT("[MiniGame] 드래그 중 테두리 발견! Point A 고정."));
        }
    }

    // -------------------------------------------------------------------------
    // [2단계] 박스 그리기 (면 방향 고정 로직)
    // -------------------------------------------------------------------------
    FVector PointA = bHasFirstPoint ? LocalFirstBoundaryPoint : LocalStartPoint;
    FVector PointB = CurrentLocalPos;

    // [핵심 수정] Point A가 어느 면에 붙어있는지로 두께 방향 결정
    // (이렇게 해야 중앙을 지나가도 박스가 뒤집히지 않음)
    bool bIsFacingX = true; // 기본값: 앞면(X)

    float DistMinX = FMath::Abs(PointA.X - MeshBounds.Min.X);
    float DistMaxX = FMath::Abs(PointA.X - MeshBounds.Max.X);
    float DistMinY = FMath::Abs(PointA.Y - MeshBounds.Min.Y);
    float DistMaxY = FMath::Abs(PointA.Y - MeshBounds.Max.Y);

    bool bOnXFace = (DistMinX < 15.0f || DistMaxX < 15.0f);
    bool bOnYFace = (DistMinY < 15.0f || DistMaxY < 15.0f);

    if (bHasFirstPoint)
    {
        // 첫 점을 찾았다면, 그 점이 붙은 면을 기준으로 함
        if (bOnXFace && !bOnYFace) bIsFacingX = true;       // 앞/뒤 면
        else if (!bOnXFace && bOnYFace) bIsFacingX = false; // 옆 면
        else 
        {
            // 코너(모서리)라서 애매하면? 드래그 방향(Delta)으로 판단
            float DeltaX = FMath::Abs(PointA.X - PointB.X);
            float DeltaY = FMath::Abs(PointA.Y - PointB.Y);
            // Y축으로 많이 움직였으면 앞면(X)을 긋는 것임
            bIsFacingX = (DeltaY > DeltaX); 
        }
    }
    else
    {
        // 아직 첫 점도 못 찾았으면(허공), 그냥 단순 Delta로 추측
        float DeltaX = FMath::Abs(PointA.X - PointB.X);
        float DeltaY = FMath::Abs(PointA.Y - PointB.Y);
        bIsFacingX = (DeltaY >= DeltaX);
    }

    FVector MinVec, MaxVec;

    if (bIsFacingX) 
    {
        // [앞면 모드] 두께는 X축 고정, YZ 평면에서 그림
        MinVec.X = MeshBounds.Min.X - 50.0f; 
        MaxVec.X = MeshBounds.Max.X + 50.0f;
        
        MinVec.Y = FMath::Min(PointA.Y, PointB.Y);
        MaxVec.Y = FMath::Max(PointA.Y, PointB.Y);
        MinVec.Z = FMath::Min(PointA.Z, PointB.Z);
        MaxVec.Z = FMath::Max(PointA.Z, PointB.Z);
    }
    else 
    {
        // [옆면 모드] 두께는 Y축 고정, XZ 평면에서 그림
        MinVec.Y = MeshBounds.Min.Y - 50.0f; 
        MaxVec.Y = MeshBounds.Max.Y + 50.0f;

        MinVec.X = FMath::Min(PointA.X, PointB.X);
        MaxVec.X = FMath::Max(PointA.X, PointB.X);
        MinVec.Z = FMath::Min(PointA.Z, PointB.Z);
        MaxVec.Z = FMath::Max(PointA.Z, PointB.Z);
    }

    CurrentPreviewBox = FBox(MinVec, MaxVec);

    // -------------------------------------------------------------------------
    // [3단계] 유효성 검증
    // -------------------------------------------------------------------------
    // 1. Point A(첫 테두리)가 있어야 함.
    // 2. 현재 Point B도 테두리여야 함.
    // 3. 둘 사이 거리가 너무 짧으면 안 됨 (점 찍기 방지).
    bool bEndOnEdge = IsOnBoundary(PointB, 15.0f);
    bool bDistanceOK = FVector::Dist(PointA, PointB) > 10.0f;

    if (bHasFirstPoint && bEndOnEdge && bDistanceOK)
    {
        bIsValidCut = true;
    }
    else
    {
        bIsValidCut = false;
    }
}

void UMDF_MiniGameComponent::EndMarking(FVector WorldLocation)
{
    if (!bIsMarking) return;

    FVector FinalLocalPos = GetLocalLocationFromWorld(WorldLocation);

    // [자동 보정] 마지막에 휙 날렸을 때 Snap
    if (bHasFirstPoint && !IsOnBoundary(FinalLocalPos, 15.0f))
    {
        FVector SnappedPos = SnapToClosestBoundary(FinalLocalPos);
        if (FVector::Dist(FinalLocalPos, SnappedPos) < 100.0f)
        {
            FinalLocalPos = SnappedPos;
            UDynamicMeshComponent* DynComp = GetOwner()->FindComponentByClass<UDynamicMeshComponent>();
            if (DynComp)
            {
                UpdateMarking(DynComp->GetComponentTransform().TransformPosition(FinalLocalPos));
            }
        }
    }
    else
    {
        UpdateMarking(WorldLocation);
    }

    // 최종 저장
    if (bIsValidCut)
    {
        FWeakSpotData NewSpot;
        NewSpot.ID = FGuid::NewGuid();
        NewSpot.LocalBox = CurrentPreviewBox;
        NewSpot.MaxHP = CalculateHPFromBox(CurrentPreviewBox);
        NewSpot.CurrentHP = NewSpot.MaxHP;
        NewSpot.bIsBroken = false;

        WeakSpots.Add(NewSpot);

        UE_LOG(LogTemp, Display, TEXT("[MiniGame] >> 영역 확정 성공! (ID: %s, HP: %.1f)"), *NewSpot.ID.ToString(), NewSpot.MaxHP);
    }
    else
    {
        if (!bHasFirstPoint)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MiniGame] >> 취소됨: 테두리를 한 번도 지나가지 않았습니다."));
        }
        else
            UE_LOG(LogTemp, Warning, TEXT("[MiniGame] >> 취소됨: 연결이 끊겼거나 너무 짧습니다."));
    }

    bIsMarking = false;
    bHasFirstPoint = false;
}

// -----------------------------------------------------------------------------
// [유틸리티]
// -----------------------------------------------------------------------------
bool UMDF_MiniGameComponent::IsOnBoundary(FVector LocalLoc, float Tolerance) const
{
    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return false;

    FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    // X, Y, Z 모든 면 체크
    bool bOnX = FMath::IsNearlyEqual(LocalLoc.X, Bounds.Min.X, Tolerance) || 
                FMath::IsNearlyEqual(LocalLoc.X, Bounds.Max.X, Tolerance);
    bool bOnY = FMath::IsNearlyEqual(LocalLoc.Y, Bounds.Min.Y, Tolerance) || 
                FMath::IsNearlyEqual(LocalLoc.Y, Bounds.Max.Y, Tolerance);
    bool bOnZ = FMath::IsNearlyEqual(LocalLoc.Z, Bounds.Min.Z, Tolerance) || 
                FMath::IsNearlyEqual(LocalLoc.Z, Bounds.Max.Z, Tolerance);

    return bOnX || bOnY || bOnZ;
}

FVector UMDF_MiniGameComponent::SnapToClosestBoundary(FVector LocalLoc) const
{
    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return LocalLoc;

    FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());
    FVector Snapped = LocalLoc;

    float DistMinX = FMath::Abs(LocalLoc.X - Bounds.Min.X);
    float DistMaxX = FMath::Abs(LocalLoc.X - Bounds.Max.X);
    float DistMinY = FMath::Abs(LocalLoc.Y - Bounds.Min.Y);
    float DistMaxY = FMath::Abs(LocalLoc.Y - Bounds.Max.Y);
    float DistMinZ = FMath::Abs(LocalLoc.Z - Bounds.Min.Z);
    float DistMaxZ = FMath::Abs(LocalLoc.Z - Bounds.Max.Z);

    float MinDist = FMath::Min(DistMinX, FMath::Min(DistMaxX, FMath::Min(DistMinY, FMath::Min(DistMaxY, FMath::Min(DistMinZ, DistMaxZ)))));

    if (MinDist == DistMinX) Snapped.X = Bounds.Min.X;
    else if (MinDist == DistMaxX) Snapped.X = Bounds.Max.X;
    else if (MinDist == DistMinY) Snapped.Y = Bounds.Min.Y;
    else if (MinDist == DistMaxY) Snapped.Y = Bounds.Max.Y;
    else if (MinDist == DistMinZ) Snapped.Z = Bounds.Min.Z;
    else if (MinDist == DistMaxZ) Snapped.Z = Bounds.Max.Z;

    return Snapped;
}

float UMDF_MiniGameComponent::CalculateHPFromBox(const FBox& Box) const
{
    FVector Size = Box.GetSize();
    float Volume = Size.X * Size.Y * Size.Z;
    return 100.0f + (Volume * HPDensityMultiplier * 0.01f);
}

void UMDF_MiniGameComponent::TryBreach(const FHitResult& HitInfo, float DamageAmount)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
    FVector LocalHit = GetLocalLocationFromWorld(HitInfo.Location);

    for (int32 i = 0; i < WeakSpots.Num(); ++i)
    {
        if (WeakSpots[i].bIsBroken) continue;
        
        // Tolerance 약간 줘서 판정
        if (WeakSpots[i].LocalBox.IsInside(LocalHit.GridSnap(1.0f)))
        {
            WeakSpots[i].CurrentHP -= DamageAmount;
            UE_LOG(LogTemp, Log, TEXT("약점 타격! HP: %.1f"), WeakSpots[i].CurrentHP);
            if (WeakSpots[i].CurrentHP <= 0.0f) ExecuteDestruction(i);
            return; 
        }
    }
}

void UMDF_MiniGameComponent::ExecuteDestruction(int32 WeakSpotIndex)
{
    if (!WeakSpots.IsValidIndex(WeakSpotIndex)) return;
    WeakSpots[WeakSpotIndex].bIsBroken = true;

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp) return;
    UDynamicMesh* TargetMesh = DynComp->GetDynamicMesh();

    UDynamicMesh* ToolMesh = NewObject<UDynamicMesh>(this); 
    FBox CutBox = WeakSpots[WeakSpotIndex].LocalBox;
    FVector Center = CutBox.GetCenter();
    FVector Extent = CutBox.GetExtent() * 1.05f; 

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(ToolMesh, FGeometryScriptPrimitiveOptions(), FTransform(Center), Extent.X * 2.f, Extent.Y * 2.f, Extent.Z * 2.f);
    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(TargetMesh, FTransform::Identity, ToolMesh, FTransform::Identity, EGeometryScriptBooleanOperation::Subtract, FGeometryScriptMeshBooleanOptions());
    
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(TargetMesh, FGeometryScriptCalculateNormalsOptions());
    DynComp->UpdateCollision();
    DynComp->NotifyMeshUpdated();
}

void UMDF_MiniGameComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp) return;

    FTransform CompTrans = DynComp->GetComponentTransform();
    
    // 마감 처리용 벽 크기
    FBox WallBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    // 1. 저장된 약점 (Cyan)
    for (const FWeakSpotData& Spot : WeakSpots)
    {
        if (Spot.bIsBroken) continue;
        FBox VisualBox = Spot.LocalBox.Overlap(WallBounds).ExpandBy(0.5f);
        DrawDebugBox(GetWorld(), CompTrans.TransformPosition(VisualBox.GetCenter()), VisualBox.GetExtent(), CompTrans.GetRotation(), FColor::Cyan, false, -1.0f, 0, 1.5f);
    }

    // 2. 드래그 중 박스 (Green/Red)
    if (bIsMarking)
    {
        FColor DrawColor = bIsValidCut ? FColor::Green : FColor::Red;
        FBox VisualPreview = CurrentPreviewBox.Overlap(WallBounds).ExpandBy(0.5f);
        DrawDebugBox(GetWorld(), CompTrans.TransformPosition(VisualPreview.GetCenter()), VisualPreview.GetExtent(), CompTrans.GetRotation(), DrawColor, false, -1.0f, 0, 3.0f);
    }
}