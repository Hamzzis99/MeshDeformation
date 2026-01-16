// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Components/MDF_MiniGameComponent.cpp

#include "Components/MDF_MiniGameComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h" 

// [★네트워크 필수] 서버-클라이언트 간 변수 복제를 위한 헤더
#include "Net/UnrealNetwork.h" 

// [Geometry Script 헤더]
#include "GeometryScript/MeshQueryFunctions.h" 
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h" 

UMDF_MiniGameComponent::UMDF_MiniGameComponent()
{
    // 미니게임 로직(드로잉 시각화)을 위해 Tick을 켭니다.
    PrimaryComponentTick.bCanEverTick = true; 

    // -------------------------------------------------------------------------
    // [네트워크 설정] 컴포넌트 복제 활성화
    // - 이 컴포넌트가 서버의 데이터를 클라이언트로 보낼 수 있도록 허용합니다.
    // -------------------------------------------------------------------------
    SetIsReplicatedByDefault(true);
}

// -----------------------------------------------------------------------------
// [네트워크 핵심] 변수 복제 규칙 정의
// - 서버에서 'WeakSpots' 배열이 변하면 클라이언트로 전파하도록 등록합니다.
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // WeakSpots 배열을 복제 대상으로 등록 (bIsBroken 상태 변화를 감지함)
    DOREPLIFETIME(UMDF_MiniGameComponent, WeakSpots);
}

// -----------------------------------------------------------------------------
// [네트워크] OnRep_WeakSpots (클라이언트 전용 실행)
// - 서버로부터 복제된 데이터를 받았을 때 클라이언트 쪽에서 자동으로 호출됩니다.
// - "서버가 어딘가를 부쉈다"는 소식을 들으면 각 클라이언트가 자기 화면의 메쉬를 깎습니다.
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::OnRep_WeakSpots()
{
    // 배열을 순회하며 서버에서 파괴되었다고(bIsBroken=true) 표시된 항목을 찾습니다.
    for (int32 i = 0; i < WeakSpots.Num(); ++i)
    {
        // 서버는 부서졌다는데, 내 화면(클라)에서는 아직 안 부서졌다면 절단 실행
        if (WeakSpots[i].bIsBroken && !LocallyProcessedIndices.Contains(i))
        {
            ApplyVisualMeshCut(i);
        }
    }
}

// -----------------------------------------------------------------------------
// [Step 5 핵심] 문지기 로직 (Gatekeeper)
// 부모(Deformable)의 기본 로직을 가로채서, 약점을 맞췄을 때만 변형을 허용합니다.
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser)
{
    // 1. 서버 권한 및 유효성 검사
    // [★네트워크 보안] 데미지 판정과 HP 차감은 반드시 '서버'에서만 처리해야 조작을 막을 수 있습니다.
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
        // [★네트워크] 약점 생성도 서버 권한이 있을 때만 배열에 추가하도록 보호합니다.
        if (GetOwner()->HasAuthority())
        {
            FWeakSpotData NewSpot;
            NewSpot.ID = FGuid::NewGuid();
            NewSpot.LocalBox = CurrentPreviewBox;
            NewSpot.MaxHP = CalculateHPFromBox(CurrentPreviewBox); // 크기에 비례한 체력 설정
            NewSpot.CurrentHP = NewSpot.MaxHP;
            NewSpot.bIsBroken = false;
            
            WeakSpots.Add(NewSpot);
            UE_LOG(LogTemp, Display, TEXT("[MiniGame] >> 영역 확정! HP: %.1f"), NewSpot.MaxHP);
        }
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
    // [★네트워크] 서버만 파괴 판정을 내립니다.
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
// [Step 6] 서버 전용 파괴 실행 (권한 체크 및 상태 전파)
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::ExecuteDestruction(int32 WeakSpotIndex)
{
    // 서버가 아니면 실행하지 않음
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (!WeakSpots.IsValidIndex(WeakSpotIndex) || WeakSpots[WeakSpotIndex].bIsBroken) return; 

    // 1. 서버에서 상태값을 변경함 (이 즉시 클라이언트들에게 데이터 복제가 시작됨)
    WeakSpots[WeakSpotIndex].bIsBroken = true;

    // 2. 서버 본인도 자신의 화면(서버 뷰)에서 메쉬를 깎아야 함
    ApplyVisualMeshCut(WeakSpotIndex);
}

// -----------------------------------------------------------------------------
// [Step 7] 시각적 절단 로직 (서버/클라이언트 공통 실행)
// - 실제 메쉬를 깎는 무거운 연산 부분입니다.
// - 데이터가 아닌 '연산 방식'을 공유함으로써 네트워크 트래픽을 아낍니다.
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::ApplyVisualMeshCut(int32 Index)
{
    if (!WeakSpots.IsValidIndex(Index)) return;

    // 중복 연산 방지 (네트워크 지연으로 인해 여러 번 호출될 수 있음)
    if (LocallyProcessedIndices.Contains(Index)) return;
    LocallyProcessedIndices.Add(Index);

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return;

    UDynamicMesh* TargetMesh = DynComp->GetDynamicMesh();

    // 1. 절단용 칼(ToolMesh) 생성
    UDynamicMesh* ToolMesh = NewObject<UDynamicMesh>(this); 
    FBox CutBox = WeakSpots[Index].LocalBox;
    
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
        ToolMesh, 
        FGeometryScriptPrimitiveOptions(), 
        FTransform(CutBox.GetCenter()), 
        CutBox.GetExtent().X * 2.0f, 
        CutBox.GetExtent().Y * 2.0f, 
        CutBox.GetExtent().Z * 2.0f
    );

    // 2. 불리언 빼기 연산 실행
    FGeometryScriptMeshBooleanOptions BoolOptions;
    BoolOptions.bFillHoles = true;       
    BoolOptions.bSimplifyOutput = false; 

    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
        TargetMesh, FTransform::Identity, ToolMesh, FTransform::Identity, 
        EGeometryScriptBooleanOperation::Subtract, BoolOptions
    );
    
    // 3. 법선, UV, 탄젠트 재계산 (모든 환경에서 동일하게 수행)
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(TargetMesh, FGeometryScriptCalculateNormalsOptions());
    
    FBox MeshBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(TargetMesh);
    FTransform BoxTransform = FTransform::Identity;
    BoxTransform.SetTranslation(MeshBounds.GetCenter());
    BoxTransform.SetScale3D(MeshBounds.GetSize());

    UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(TargetMesh, 0, BoxTransform, FGeometryScriptMeshSelection());
    UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents(TargetMesh, FGeometryScriptTangentsOptions());
    
    // -------------------------------------------------------------------------
    // [렌더링/컬링 해결] 모든 클라이언트에서 화면을 최신 상태로 갱신
    // -------------------------------------------------------------------------
    DynComp->MarkRenderTransformDirty(); 
    DynComp->NotifyMeshUpdated();        
    DynComp->UpdateCollision(true);      
    DynComp->MarkRenderStateDirty(); // 가시성 상태 리셋 (중요)

    UE_LOG(LogTemp, Log, TEXT("[MDF Sync] 로컬 절단 시각화 연산 완료 (Index: %d)"), Index);
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

    float MinDist = FMath::Min(FMath::Min(DistMinX, DistMaxX), FMath::Min(DistMinY, DistMaxY));

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