// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Components/MDF_MiniGameComponent.cpp

#include "Components/MDF_MiniGameComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h" 

// [Geometry Script 헤더]
#include "GeometryScript/MeshQueryFunctions.h" 
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"

// [★필수 추가] 이 헤더가 없으면 'Options' 구조체 에러가 뜹니다. (탄젠트 계산용)
#include "GeometryScript/GeometryScriptTypes.h" 

UMDF_MiniGameComponent::UMDF_MiniGameComponent()
{
    // 미니게임 로직(드로잉 시각화)을 위해 Tick을 켭니다.
    PrimaryComponentTick.bCanEverTick = true; 
}

// -----------------------------------------------------------------------------
// [Step 5 핵심] 문지기 로직 (Gatekeeper)
// 부모(Deformable)의 기본 로직을 가로채서, 약점을 맞췄을 때만 변형을 허용합니다.
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser)
{
    // 1. 서버 권한 및 유효성 검사
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    // 2. 약점 명중 여부 확인 (TryBreach 내부에서 HP 차감 및 파괴 판정 수행)
    FHitResult HitInfo;
    HitInfo.Location = HitLocation;
    
    bool bHitWeakSpot = TryBreach(HitInfo, Damage);

    if (bHitWeakSpot)
    {
        // [핵심 로직] 약점을 맞췄을 때만 부모의 변형 로직을 수행합니다.
        UE_LOG(LogTemp, Log, TEXT("[MDF Gatekeeper] 약점 명중! (부모 태그 검사 우회 -> 강제 적용)"));

        // 3. 부모(Deformable)가 처리할 수 있도록 데이터 포장
        FVector LocalHitPos = GetLocalLocationFromWorld(HitLocation);
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

        // 4. 변형 대기열에 추가 및 즉시 처리
        HitQueue.Add(NewHit);
        ProcessDeformationBatch();
    }
    // 약점이 아니면 아무 일도 일어나지 않음 (무적 상태)
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
// [Step 1~3] 리더(Leader)의 마킹 로직
// 플레이어가 드래그하여 절단할 영역(Box)을 설계하는 단계
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::StartMarking(FVector WorldLocation)
{
    bIsMarking = true;
    bIsValidCut = false; 
    bHasFirstPoint = false;
    
    // 시작점 기록
    LocalStartPoint = GetLocalLocationFromWorld(WorldLocation);

    UE_LOG(LogTemp, Log, TEXT("--- [MiniGame] 드래그 시작 (Local: %s) ---"), *LocalStartPoint.ToString());

    // 시작점이 테두리(Boundary) 근처인지 확인
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
    
    // 메쉬의 전체 크기(경계) 가져오기
    FBox MeshBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    // 1. 첫 시작점이 테두리가 아니었다면, 드래그 중에 테두리를 찾음
    if (!bHasFirstPoint)
    {
        if (IsOnBoundary(CurrentLocalPos, 15.0f))
        {
            bHasFirstPoint = true;
            LocalFirstBoundaryPoint = CurrentLocalPos;
            UE_LOG(LogTemp, Log, TEXT("[MiniGame] 드래그 중 테두리 진입!"));
        }
    }

    // 2. 박스 생성 로직 (핵심)
    FVector PointA = LocalStartPoint;
    FVector PointB = CurrentLocalPos;

    // 어느 면을 보고 있는지 판별 (가장 가까운 면 찾기)
    float DistMinX = FMath::Abs(PointA.X - MeshBounds.Min.X);
    float DistMaxX = FMath::Abs(PointA.X - MeshBounds.Max.X);
    float DistMinY = FMath::Abs(PointA.Y - MeshBounds.Min.Y);
    float DistMaxY = FMath::Abs(PointA.Y - MeshBounds.Max.Y);
    float MinDist = FMath::Min(FMath::Min(DistMinX, DistMaxX), FMath::Min(DistMinY, DistMaxY));

    FVector MinVec, MaxVec;
    
    // 높이(Z): 사용자가 그은 곳부터 바닥까지 (-50.0f 여유분)
    float UserDrawHeight = FMath::Max(PointA.Z, PointB.Z);
    float BottomZ = MeshBounds.Min.Z - 50.0f;

    // -------------------------------------------------------------------------
    // [깊이 확장] 벽이 찌그러져도 판정이 끊기지 않도록 깊이(Depth)를 아주 크게 잡습니다.
    // -------------------------------------------------------------------------
    if (MinDist == DistMinX || MinDist == DistMaxX) // 앞/뒤 면인 경우
    {
        MinVec.X = MeshBounds.Min.X - 500.0f; // 깊이(X) 확장
        MaxVec.X = MeshBounds.Max.X + 500.0f;
        MinVec.Y = FMath::Min(PointA.Y, PointB.Y); // 너비(Y)는 드래그한 만큼
        MaxVec.Y = FMath::Max(PointA.Y, PointB.Y);
        MinVec.Z = BottomZ;
        MaxVec.Z = UserDrawHeight;
    }
    else // 좌/우 면인 경우
    {
        MinVec.Y = MeshBounds.Min.Y - 500.0f; // 깊이(Y) 확장
        MaxVec.Y = MeshBounds.Max.Y + 500.0f;
        MinVec.X = FMath::Min(PointA.X, PointB.X); // 너비(X)는 드래그한 만큼
        MaxVec.X = FMath::Max(PointA.X, PointB.X);
        MinVec.Z = BottomZ;
        MaxVec.Z = UserDrawHeight;
    }

    CurrentPreviewBox = FBox(MinVec, MaxVec);

    // 3. 유효성 검사 (너무 짧으면 안 됨)
    float DragDistance = FVector::Dist(PointA, PointB);
    bool bDistanceOK = (DragDistance > 10.0f); 

    if (bHasFirstPoint && IsOnBoundary(PointB, 15.0f) && bDistanceOK)
    {
        bIsValidCut = true; // 초록색 (유효)
    }
    else
    {
        bIsValidCut = false; // 빨간색 (무효)
    }
}

void UMDF_MiniGameComponent::EndMarking(FVector WorldLocation)
{
    if (!bIsMarking) return;

    FVector FinalLocalPos = GetLocalLocationFromWorld(WorldLocation);
    
    // 드래그 끝점이 테두리 근처라면 자동으로 딱 붙여줌 (Snap)
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

    // 최종 유효성 확인 후 데이터 저장
    if (bIsValidCut)
    {
        FWeakSpotData NewSpot;
        NewSpot.ID = FGuid::NewGuid();
        NewSpot.LocalBox = CurrentPreviewBox;
        NewSpot.MaxHP = CalculateHPFromBox(CurrentPreviewBox); // 크기에 비례한 체력 설정
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
// [Step 4~5] 슈터(Shooter)의 파괴 로직
// 설정된 약점을 사격하여 HP를 깎고 파괴를 유발함
// -----------------------------------------------------------------------------
bool UMDF_MiniGameComponent::TryBreach(const FHitResult& HitInfo, float DamageAmount)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return false;
    FVector LocalHit = GetLocalLocationFromWorld(HitInfo.Location);

    // 저장된 모든 약점 검사
    for (int32 i = 0; i < WeakSpots.Num(); ++i)
    {
        if (WeakSpots[i].bIsBroken) continue; // 이미 파괴된 곳은 패스

        // 히트 위치가 약점 박스 안에 있는지 확인 (오차범위 5.0f 허용)
        if (WeakSpots[i].LocalBox.ExpandBy(5.0f).IsInside(LocalHit))
        {
            // --- 명중! ---
            WeakSpots[i].CurrentHP -= DamageAmount;
            UE_LOG(LogTemp, Display, TEXT("   >>> [HIT!] 약점 명중! (Index: %d, 남은HP: %.1f)"), i, WeakSpots[i].CurrentHP);

            // HP가 0이 되면 절단(Destruction) 실행
            if (WeakSpots[i].CurrentHP <= 0.0f)
            {
                UE_LOG(LogTemp, Error, TEXT("   >>> [DESTROY] 파괴 조건 달성! 절단 실행!"));
                ExecuteDestruction(i);
            }
            return true; // 부모에게 "유효타"라고 알림 (찌그러짐 효과 발생)
        }
    }
    return false; // 약점 아님 (변형 없음)
}

// -----------------------------------------------------------------------------
// [Step 6] 절단 실행 (정밀 절단 & 렌더링 보정)
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::ExecuteDestruction(int32 WeakSpotIndex)
{
    if (!WeakSpots.IsValidIndex(WeakSpotIndex)) return;
    if (WeakSpots[WeakSpotIndex].bIsBroken) return; 

    // 상태 업데이트
    WeakSpots[WeakSpotIndex].bIsBroken = true;

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return;

    UDynamicMesh* TargetMesh = DynComp->GetDynamicMesh();

    // 1. 절단용 칼(Tool Mesh) 만들기
    UDynamicMesh* ToolMesh = NewObject<UDynamicMesh>(this); 
    FBox CutBox = WeakSpots[WeakSpotIndex].LocalBox;
    FVector Center = CutBox.GetCenter();
    FVector Extent = CutBox.GetExtent();
    
    // [중요] 리더가 그린 라인 크기 '그대로' 적용 (정밀 절단)
    // 깊이(관통 방향)는 Marking 단계에서 이미 충분히 길게 설정되어 있음.
    float SizeX = Extent.X * 2.0f;
    float SizeY = Extent.Y * 2.0f;
    float SizeZ = Extent.Z * 2.0f;

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
        ToolMesh, 
        FGeometryScriptPrimitiveOptions(), 
        FTransform(Center), 
        SizeX, SizeY, SizeZ
    );

    // 2. 빼기 연산 (Boolean Subtract)
    FGeometryScriptMeshBooleanOptions BoolOptions;
    BoolOptions.bFillHoles = true;       // 절단면 메우기 (Solid 유지)
    BoolOptions.bSimplifyOutput = false; // 형태 단순화 끄기 (정확한 박스 모양)

    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
        TargetMesh, FTransform::Identity, ToolMesh, FTransform::Identity, 
        EGeometryScriptBooleanOperation::Subtract, BoolOptions
    );
    
    // -------------------------------------------------------------------------
    // [Step 3] 마무리 및 렌더링 업데이트 (핵심)
    // -------------------------------------------------------------------------
    
    // 법선(Normal) 재계산: 빛의 반사 방향 갱신
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(TargetMesh, FGeometryScriptCalculateNormalsOptions());
    
    // [★핵심] 탄젠트(Tangent) 재계산
    // 움직이는 물체(Movable)가 빛을 받을 때 투명해지거나 검게 나오는 버그를 방지합니다.
    UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents(TargetMesh, FGeometryScriptTangentsOptions());
    
    // 충돌체 및 렌더링 알림
    DynComp->UpdateCollision(true); 
    DynComp->NotifyMeshUpdated();

    UE_LOG(LogTemp, Warning, TEXT("[MDF] >>> 정밀 절단 완료 (탄젠트 포함) (Index: %d)"), WeakSpotIndex);
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

    // 스케일을 고려한 오차 범위 계산
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

    // 가장 가까운 면으로 좌표 강제 이동(Snap)
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
    
    // 부피에 비례하여 HP 설정 (너무 큰 영역은 부수기 힘들게)
    return 100.0f + (LocalVolume * ScaleFactor * HPDensityMultiplier * 0.005f);
}

void UMDF_MiniGameComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp) return;

    FTransform CompTrans = DynComp->GetComponentTransform();
    FVector WorldScale = CompTrans.GetScale3D();
    
    FBox WallBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    // 1. 확정된 약점 표시 (Cyan 색상)
    for (const FWeakSpotData& Spot : WeakSpots)
    {
        if (Spot.bIsBroken) continue;
        FBox VisualBox = Spot.LocalBox.Overlap(WallBounds).ExpandBy(0.5f);
        FVector ScaledExtent = VisualBox.GetExtent() * WorldScale;
        DrawDebugBox(GetWorld(), CompTrans.TransformPosition(VisualBox.GetCenter()), ScaledExtent, CompTrans.GetRotation(), FColor::Cyan, false, -1.0f, 0, 1.5f);
    }

    // 2. 현재 드래그 중인 박스 표시 (Green=유효, Red=무효)
    if (bIsMarking)
    {
        FColor DrawColor = bIsValidCut ? FColor::Green : FColor::Red;
        FBox VisualPreview = CurrentPreviewBox.Overlap(WallBounds).ExpandBy(0.5f);
        FVector ScaledExtent = VisualPreview.GetExtent() * WorldScale;
        DrawDebugBox(GetWorld(), CompTrans.TransformPosition(VisualPreview.GetCenter()), ScaledExtent, CompTrans.GetRotation(), DrawColor, false, -1.0f, 0, 3.0f);
    }
}