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

// -----------------------------------------------------------------------------
// [Step 5 핵심] 문지기 로직 (Gatekeeper)
// 부모(Deformable)의 무조건적인 변형을 막고, 약점일 때만 허용합니다.
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser)
{
    // 1. 서버 권한 및 유효성 검사 (S3: 안전성)
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    // 2. 약점 명중 여부 확인 (TryBreach 내부에서 HP 차감 수행됨)
    FHitResult HitInfo;
    HitInfo.Location = HitLocation;
    
    bool bHitWeakSpot = TryBreach(HitInfo, Damage);

    if (bHitWeakSpot)
    {
        // [핵심 수정] Super::HandlePointDamage를 호출하지 않습니다.
        // 이유: 부모 함수에는 태그 검사(MDF_Test)가 있어 플레이어의 공격을 무시하기 때문입니다.
        // 대신, 약점을 맞췄으므로 강제로 변형 대기열에 추가합니다.

        UE_LOG(LogTemp, Log, TEXT("[MDF Gatekeeper] 약점 명중! (태그 검사 우회 -> 강제 적용)"));

        // 3. 부모의 로직 수동 구현 (데이터 생성)
        // 좌표 변환: 부모의 함수를 재사용하거나 직접 변환
        FVector LocalHitPos = GetLocalLocationFromWorld(HitLocation);
        
        // 방향 변환
        FVector LocalDir = FVector::ForwardVector;
        UDynamicMeshComponent* MeshComp = GetOwner()->FindComponentByClass<UDynamicMeshComponent>();
        if (IsValid(MeshComp))
        {
            LocalDir = MeshComp->GetComponentTransform().InverseTransformVector(ShotFromDirection);
        }

        FMDFHitData NewHit(
            LocalHitPos,
            LocalDir,
            Damage,
            DamageType ? DamageType->GetClass() : nullptr
        );

        // 4. 대기열(HitQueue)에 직접 추가 (부모의 protected 멤버 접근)
        HitQueue.Add(NewHit);

        // 5. 즉시 처리 (MiniGame은 최적화보다 반응성이 중요하므로 타이머 없이 즉시 실행)
        ProcessDeformationBatch();
    }
    else
    {
        // 약점이 아니면 무시 (부모 호출 X -> 변형 X)
        // UE_LOG(LogTemp, Verbose, TEXT("[MDF Gatekeeper] 약점 아님 -> 변형 차단"));
    }
}

// -----------------------------------------------------------------------------
// [좌표 변환 및 유틸리티]
// -----------------------------------------------------------------------------
FVector UMDF_MiniGameComponent::GetLocalLocationFromWorld(FVector WorldLoc) const
{
    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (DynComp)
    {
        return DynComp->GetComponentTransform().InverseTransformPosition(WorldLoc);
    }
    return GetOwner() ? GetOwner()->GetActorTransform().InverseTransformPosition(WorldLoc) : WorldLoc;
}

// -----------------------------------------------------------------------------
// [Step 1~3] 리더의 마킹 로직
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::StartMarking(FVector WorldLocation)
{
    bIsMarking = true;
    bIsValidCut = false; 
    bHasFirstPoint = false;

    // 시작점 기록
    LocalStartPoint = GetLocalLocationFromWorld(WorldLocation);

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

    float DistMinX = FMath::Abs(PointA.X - MeshBounds.Min.X);
    float DistMaxX = FMath::Abs(PointA.X - MeshBounds.Max.X);
    float DistMinY = FMath::Abs(PointA.Y - MeshBounds.Min.Y);
    float DistMaxY = FMath::Abs(PointA.Y - MeshBounds.Max.Y);

    float MinDist = FMath::Min(FMath::Min(DistMinX, DistMaxX), FMath::Min(DistMinY, DistMaxY));

    FVector MinVec, MaxVec;

    // 높이(Z) 설정: 사용자가 그은 곳(Max) ~ 바닥(Min)
    float UserDrawHeight = FMath::Max(PointA.Z, PointB.Z); 
    float BottomZ = MeshBounds.Min.Z - 50.0f;              

    // --- X면 (앞/뒤) ---
    if (MinDist == DistMinX || MinDist == DistMaxX)
    {
        MinVec.X = MeshBounds.Min.X - 50.0f; 
        MaxVec.X = MeshBounds.Max.X + 50.0f;
        MinVec.Y = FMath::Min(PointA.Y, PointB.Y); 
        MaxVec.Y = FMath::Max(PointA.Y, PointB.Y);
        MinVec.Z = BottomZ;
        MaxVec.Z = UserDrawHeight;
    }
    // --- Y면 (좌/우) ---
    else 
    {
        MinVec.Y = MeshBounds.Min.Y - 50.0f; 
        MaxVec.Y = MeshBounds.Max.Y + 50.0f;
        MinVec.X = FMath::Min(PointA.X, PointB.X); 
        MaxVec.X = FMath::Max(PointA.X, PointB.X);
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
                FVector WorldSnapped = DynComp->GetComponentTransform().TransformPosition(FinalLocalPos);
                UpdateMarking(WorldSnapped);
            }
        }
    }
    else
    {
        UpdateMarking(WorldLocation);
    }

    // 데이터 저장
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
        if (!bHasFirstPoint) {UE_LOG(LogTemp, Warning, TEXT("[MiniGame] 취소: 테두리 미진입"));}
        
        else UE_LOG(LogTemp, Warning, TEXT("[MiniGame] 취소: 연결 끊김/짧음"));
    }

    bIsMarking = false;
    bHasFirstPoint = false;
}

// -----------------------------------------------------------------------------
// [Step 4~5] 슈터의 파괴 로직
// -----------------------------------------------------------------------------
bool UMDF_MiniGameComponent::TryBreach(const FHitResult& HitInfo, float DamageAmount)
{
    // 1. 서버 권한 체크
    if (GetOwner() && !GetOwner()->HasAuthority()) return false;

    FVector LocalHit = GetLocalLocationFromWorld(HitInfo.Location);

    // [디버그] 현재 맞은 로컬 좌표 확인
    // UE_LOG(LogTemp, Warning, TEXT("--- [TryBreach] 타격 시도: %s ---"), *LocalHit.ToString());

    // 2. 저장된 약점들을 순회
    for (int32 i = 0; i < WeakSpots.Num(); ++i)
    {
        if (WeakSpots[i].bIsBroken) continue;

        // [관용구] 오차 범위(5.0f) 적용하여 검사
        if (WeakSpots[i].LocalBox.ExpandBy(5.0f).IsInside(LocalHit))
        {
            // --- 명중! ---
            WeakSpots[i].CurrentHP -= DamageAmount;

            UE_LOG(LogTemp, Display, TEXT("   >>> [HIT!] 약점 명중! (Index: %d, 남은HP: %.1f)"), i, WeakSpots[i].CurrentHP);

            // 파괴 조건
            if (WeakSpots[i].CurrentHP <= 0.0f)
            {
                UE_LOG(LogTemp, Error, TEXT("   >>> [DESTROY] 파괴 조건 달성! 절단 실행!"));
                ExecuteDestruction(i);
            }
            
            // 하나라도 맞았으면 true 반환 (부모 찌그러짐 허용)
            return true; 
        }
    }

    // 아무 약점도 못 맞춤 (부모 찌그러짐 차단)
    return false;
}

void UMDF_MiniGameComponent::ExecuteDestruction(int32 WeakSpotIndex)
{
    if (!WeakSpots.IsValidIndex(WeakSpotIndex)) return;
    WeakSpots[WeakSpotIndex].bIsBroken = true;

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp) return;
    UDynamicMesh* TargetMesh = DynComp->GetDynamicMesh();

    // 임시 메쉬 생성 및 Boolean Subtract
    UDynamicMesh* ToolMesh = NewObject<UDynamicMesh>(this); 
    FBox CutBox = WeakSpots[WeakSpotIndex].LocalBox;
    FVector Center = CutBox.GetCenter();
    FVector Extent = CutBox.GetExtent() * 1.05f; 

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(ToolMesh, FGeometryScriptPrimitiveOptions(), FTransform(Center), Extent.X * 2.f, Extent.Y * 2.f, Extent.Z * 2.f);
    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(TargetMesh, FTransform::Identity, ToolMesh, FTransform::Identity, EGeometryScriptBooleanOperation::Subtract, FGeometryScriptMeshBooleanOptions());
    
    // 업데이트
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(TargetMesh, FGeometryScriptCalculateNormalsOptions());
    DynComp->UpdateCollision();
    DynComp->NotifyMeshUpdated();
}

// -----------------------------------------------------------------------------
// [유틸리티 & Tick]
// -----------------------------------------------------------------------------
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
    
    return 100.0f + (LocalVolume * ScaleFactor * HPDensityMultiplier * 0.005f);
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