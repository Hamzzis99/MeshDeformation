// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Components/MDF_MiniGameComponent.cpp

#include "Components/MDF_MiniGameComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DrawDebugHelpers.h"

// [Geometry Script 헤더]
#include "GeometryScript/MeshQueryFunctions.h" 
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"

UMDF_MiniGameComponent::UMDF_MiniGameComponent()
{
    PrimaryComponentTick.bCanEverTick = true; 
}

// [좌표 변환]
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
    bIsMarking = true;
    bIsValidCut = false; 
    bHasFirstPoint = false;

    // 시작점 기록
    LocalStartPoint = GetLocalLocationFromWorld(WorldLocation);

    // 로그 출력
    UE_LOG(LogTemp, Log, TEXT("--- [MiniGame] 드래그 시작 (Local: %s) ---"), *LocalStartPoint.ToString());

    if (IsOnBoundary(LocalStartPoint, 15.0f))
    {
        bHasFirstPoint = true;
        LocalFirstBoundaryPoint = LocalStartPoint;
    }
}

void UMDF_MiniGameComponent::UpdateMarking(FVector WorldLocation)
{
    if (!bIsMarking) return;

    FVector CurrentLocalPos = GetLocalLocationFromWorld(WorldLocation);

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return;
    
    FBox MeshBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    // 1. 첫 테두리 점 찾기
    if (!bHasFirstPoint)
    {
        if (IsOnBoundary(CurrentLocalPos, 15.0f))
        {
            bHasFirstPoint = true;
            LocalFirstBoundaryPoint = CurrentLocalPos;
            UE_LOG(LogTemp, Log, TEXT("[MiniGame] 드래그 중 테두리 진입!"));
        }
    }

    // 2. [핵심 로직] 아래쪽으로 자동 확장 (Guillotine Logic)
    FVector PointA = LocalStartPoint;
    FVector PointB = CurrentLocalPos;

    // 면 감지 (X면 vs Y면)
    float DistMinX = FMath::Abs(PointA.X - MeshBounds.Min.X);
    float DistMaxX = FMath::Abs(PointA.X - MeshBounds.Max.X);
    float DistMinY = FMath::Abs(PointA.Y - MeshBounds.Min.Y);
    float DistMaxY = FMath::Abs(PointA.Y - MeshBounds.Max.Y);
    // Z축은 이제 무시 (높이는 사용자가 정하는 게 아니라 바닥까지 깔리니까)

    // 가장 가까운 면 찾기
    float MinDist = FMath::Min(FMath::Min(DistMinX, DistMaxX), FMath::Min(DistMinY, DistMaxY));

    FVector MinVec, MaxVec;

    // -------------------------------------------------------------------------
    // [수정됨] 높이(Z) 설정: 사용자가 그은 곳(Max) ~ 바닥(Min)
    // -------------------------------------------------------------------------
    float UserDrawHeight = FMath::Max(PointA.Z, PointB.Z); // 사용자가 그은 선의 높이
    float BottomZ = MeshBounds.Min.Z - 50.0f;              // 메쉬의 바닥보다 더 아래 (확실히 뚫기 위해)

    // --- X면 (앞/뒤) ---
    if (MinDist == DistMinX || MinDist == DistMaxX)
    {
        // 두께(X): 벽 전체 관통
        MinVec.X = MeshBounds.Min.X - 50.0f; 
        MaxVec.X = MeshBounds.Max.X + 50.0f;
        
        // 너비(Y): 드래그 범위
        MinVec.Y = FMath::Min(PointA.Y, PointB.Y); 
        MaxVec.Y = FMath::Max(PointA.Y, PointB.Y);
        
        // [변경] 높이(Z): 그은 곳에서 바닥까지
        MinVec.Z = BottomZ;
        MaxVec.Z = UserDrawHeight;
    }
    // --- Y면 (좌/우) ---
    else 
    {
        // 두께(Y): 벽 전체 관통
        MinVec.Y = MeshBounds.Min.Y - 50.0f; 
        MaxVec.Y = MeshBounds.Max.Y + 50.0f;

        // 너비(X): 드래그 범위
        MinVec.X = FMath::Min(PointA.X, PointB.X); 
        MaxVec.X = FMath::Max(PointA.X, PointB.X);
        
        // [변경] 높이(Z): 그은 곳에서 바닥까지
        MinVec.Z = BottomZ;
        MaxVec.Z = UserDrawHeight;
    }

    CurrentPreviewBox = FBox(MinVec, MaxVec);

    // 3. 유효성 검사
    float DragDistance = FVector::Dist(PointA, PointB);
    bool bDistanceOK = (DragDistance > 10.0f); 

    if (bHasFirstPoint && IsOnBoundary(PointB, 15.0f) && bDistanceOK)
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
    
    // 자동 보정 (Snap)
    if (bHasFirstPoint && !IsOnBoundary(FinalLocalPos, 15.0f))
    {
        FVector SnappedPos = SnapToClosestBoundary(FinalLocalPos);
        if (FVector::Dist(FinalLocalPos, SnappedPos) < 100.0f)
        {
            FinalLocalPos = SnappedPos;
            UDynamicMeshComponent* DynComp = GetOwner()->FindComponentByClass<UDynamicMeshComponent>();
            if (DynComp)
            {
                // 보정된 위치로 업데이트 (이때 바닥까지 내려가는 로직도 자동 적용됨)
                FVector WorldSnapped = DynComp->GetComponentTransform().TransformPosition(FinalLocalPos);
                UpdateMarking(WorldSnapped);
            }
        }
    }
    else
    {
        UpdateMarking(WorldLocation);
    }

    // 저장
    if (bIsValidCut)
    {
        FWeakSpotData NewSpot;
        NewSpot.ID = FGuid::NewGuid();
        NewSpot.LocalBox = CurrentPreviewBox;
        NewSpot.MaxHP = CalculateHPFromBox(CurrentPreviewBox);
        NewSpot.CurrentHP = NewSpot.MaxHP;
        NewSpot.bIsBroken = false;

        WeakSpots.Add(NewSpot);

        UE_LOG(LogTemp, Display, TEXT("[MiniGame] >> 영역 확정 (바닥까지 포함)! HP: %.1f"), NewSpot.MaxHP);
    }
    else
    {
        if (!bHasFirstPoint)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MiniGame] 취소: 테두리 미진입"));
        }
        else
            UE_LOG(LogTemp, Warning, TEXT("[MiniGame] 취소: 연결 끊김/짧음"));
    }

    bIsMarking = false;
    bHasFirstPoint = false;
}

// [유틸리티]
bool UMDF_MiniGameComponent::IsOnBoundary(FVector LocalLoc, float Tolerance) const
{
    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return false;

    FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());
    FVector Scale = DynComp->GetComponentScale();

    float TolX = Tolerance / FMath::Max(Scale.X, 0.001f);
    float TolY = Tolerance / FMath::Max(Scale.Y, 0.001f);
    float TolZ = Tolerance / FMath::Max(Scale.Z, 0.001f);

    bool bOnX = FMath::IsNearlyEqual(LocalLoc.X, Bounds.Min.X, TolX) || FMath::IsNearlyEqual(LocalLoc.X, Bounds.Max.X, TolX);
    bool bOnY = FMath::IsNearlyEqual(LocalLoc.Y, Bounds.Min.Y, TolY) || FMath::IsNearlyEqual(LocalLoc.Y, Bounds.Max.Y, TolY);
    bool bOnZ = FMath::IsNearlyEqual(LocalLoc.Z, Bounds.Min.Z, TolZ) || FMath::IsNearlyEqual(LocalLoc.Z, Bounds.Max.Z, TolZ);

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
    float LocalVolume = Size.X * Size.Y * Size.Z;
    FVector Scale = GetOwner() ? GetOwner()->GetActorScale3D() : FVector::OneVector;
    float ScaleFactor = Scale.X * Scale.Y * Scale.Z;
    
    // 부피가 커지므로 계수를 조금 낮춰서 밸런스를 맞춤 (0.01 -> 0.005)
    return 100.0f + (LocalVolume * ScaleFactor * HPDensityMultiplier * 0.005f);
}

void UMDF_MiniGameComponent::TryBreach(const FHitResult& HitInfo, float DamageAmount)
{
    // 1. 서버 권한 체크
    if (GetOwner() && !GetOwner()->HasAuthority()) return;

    // 월드 좌표 -> 로컬 좌표 변환
    FVector LocalHit = GetLocalLocationFromWorld(HitInfo.Location);

    // [디버그] 현재 맞은 로컬 좌표 확인
    UE_LOG(LogTemp, Warning, TEXT("--- [TryBreach] 타격 시도 ---"));
    UE_LOG(LogTemp, Warning, TEXT("   > 로컬 타격 위치: %s"), *LocalHit.ToString());

    bool bHitAny = false;

    // 2. 저장된 약점들을 순회
    for (int32 i = 0; i < WeakSpots.Num(); ++i)
    {
        if (WeakSpots[i].bIsBroken) continue;

        // [디버그] 박스 범위 찍어보기
        /*
        UE_LOG(LogTemp, Log, TEXT("   > 약점[%d] 범위: Min %s ~ Max %s"), 
            i, *WeakSpots[i].LocalBox.Min.ToString(), *WeakSpots[i].LocalBox.Max.ToString());
        */

        // [수정] 박스를 약간 키워서(Expand) 검사 (관용도 5cm)
        // 표면에 맞았을 때 오차로 인해 안 맞았다고 뜨는 것을 방지함
        if (WeakSpots[i].LocalBox.ExpandBy(5.0f).IsInside(LocalHit))
        {
            // 3. HP 차감
            WeakSpots[i].CurrentHP -= DamageAmount;
            bHitAny = true;

            UE_LOG(LogTemp, Display, TEXT("   >>> [HIT!] 약점 명중! (Index: %d)"), i);
            UE_LOG(LogTemp, Display, TEXT("       남은 HP: %.1f / %.1f (데미지: %.1f)"), 
                WeakSpots[i].CurrentHP, WeakSpots[i].MaxHP, DamageAmount);

            // 4. 파괴 조건
            if (WeakSpots[i].CurrentHP <= 0.0f)
            {
                UE_LOG(LogTemp, Error, TEXT("   >>> [DESTROY] 파괴 조건 달성! 메쉬 절단 시작!"));
                ExecuteDestruction(i);
            }
            
            // 한 발에 여러 박스가 겹쳐있으면 다 데미지를 줄지, 하나만 줄지 결정 (일단 return해서 하나만)
            return; 
        }
    }

    if (!bHitAny)
    {
        UE_LOG(LogTemp, Warning, TEXT("   > 실패: 범위 안에 들어오지 않음 (오차범위 5.0f 적용됨)"));
    }
}

void UMDF_MiniGameComponent::ExecuteDestruction(int32 WeakSpotIndex)
{
    if (!WeakSpots.IsValidIndex(WeakSpotIndex)) return;
    WeakSpots[WeakSpotIndex].bIsBroken = true;

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp) return;
    UDynamicMesh* TargetMesh = DynComp->GetDynamicMesh();

    // 임시 메쉬 생성
    UDynamicMesh* ToolMesh = NewObject<UDynamicMesh>(this); 
    FBox CutBox = WeakSpots[WeakSpotIndex].LocalBox;
    FVector Center = CutBox.GetCenter();
    FVector Extent = CutBox.GetExtent() * 1.05f; // 약간 더 크게

    // Boolean Subtract
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(ToolMesh, FGeometryScriptPrimitiveOptions(), FTransform(Center), Extent.X * 2.f, Extent.Y * 2.f, Extent.Z * 2.f);
    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(TargetMesh, FTransform::Identity, ToolMesh, FTransform::Identity, EGeometryScriptBooleanOperation::Subtract, FGeometryScriptMeshBooleanOptions());
    
    // 업데이트
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
    FVector WorldScale = CompTrans.GetScale3D();
    
    // 마감 처리용 벽 크기
    FBox WallBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    // 1. 저장된 약점 (Cyan)
    for (const FWeakSpotData& Spot : WeakSpots)
    {
        if (Spot.bIsBroken) continue;
        FBox VisualBox = Spot.LocalBox.Overlap(WallBounds).ExpandBy(0.5f);
        FVector ScaledExtent = VisualBox.GetExtent() * WorldScale;
        DrawDebugBox(GetWorld(), CompTrans.TransformPosition(VisualBox.GetCenter()), ScaledExtent, CompTrans.GetRotation(), FColor::Cyan, false, -1.0f, 0, 1.5f);
    }

    // 2. 드래그 중 (Green/Red)
    if (bIsMarking)
    {
        FColor DrawColor = bIsValidCut ? FColor::Green : FColor::Red;
        FBox VisualPreview = CurrentPreviewBox.Overlap(WallBounds).ExpandBy(0.5f);
        FVector ScaledExtent = VisualPreview.GetExtent() * WorldScale;
        DrawDebugBox(GetWorld(), CompTrans.TransformPosition(VisualPreview.GetCenter()), ScaledExtent, CompTrans.GetRotation(), DrawColor, false, -1.0f, 0, 3.0f);
    }
}