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
    PrimaryComponentTick.bCanEverTick = true; 
    SetIsReplicatedByDefault(true);
}

void UMDF_MiniGameComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMDF_MiniGameComponent, WeakSpots);
}

void UMDF_MiniGameComponent::OnRep_WeakSpots()
{
    for (int32 i = 0; i < WeakSpots.Num(); ++i)
    {
        if (WeakSpots[i].bIsBroken && !LocallyProcessedIndices.Contains(i))
        {
            ApplyVisualMeshCut(i);
        }
    }
}

// -----------------------------------------------------------------------------
// [핵심 수정] HandlePointDamage - 부모 배칭 시스템 활용
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::HandlePointDamage(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser)
{
    // 1. 서버 권한 체크
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    // 2. 약점 명중 여부 확인
    FHitResult HitInfo;
    HitInfo.Location = HitLocation;
    
    bool bHitWeakSpot = TryBreach(HitInfo, Damage);

    if (bHitWeakSpot)
    {
        UE_LOG(LogTemp, Log, TEXT("[MDF Gatekeeper] 약점 명중! 찌그러짐 적용"));

        // 3. 좌표 변환
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

        // 4. [수정] 부모의 배칭 시스템 활용 - 헬퍼 함수 사용
        HitQueue.Add(NewHit);
        StartBatchTimer();
    }
    // 약점이 아니면 아무 일도 일어나지 않음
}

// -----------------------------------------------------------------------------
// [좌표 변환]
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
// [마킹 로직]
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::StartMarking(FVector WorldLocation)
{
    bIsMarking = true;
    bIsValidCut = false; 
    bHasFirstPoint = false;
    
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

    if (!bHasFirstPoint)
    {
        if (IsOnBoundary(CurrentLocalPos, 15.0f))
        {
            bHasFirstPoint = true;
            LocalFirstBoundaryPoint = CurrentLocalPos;
            UE_LOG(LogTemp, Log, TEXT("[MiniGame] 드래그 중 테두리 진입!"));
        }
    }

    FVector PointA = LocalStartPoint;
    FVector PointB = CurrentLocalPos;

    float DistMinX = FMath::Abs(PointA.X - MeshBounds.Min.X);
    float DistMaxX = FMath::Abs(PointA.X - MeshBounds.Max.X);
    float DistMinY = FMath::Abs(PointA.Y - MeshBounds.Min.Y);
    float DistMaxY = FMath::Abs(PointA.Y - MeshBounds.Max.Y);
    float MinDist = FMath::Min(FMath::Min(DistMinX, DistMaxX), FMath::Min(DistMinY, DistMaxY));

    FVector MinVec, MaxVec;
    
    float UserDrawHeight = FMath::Max(PointA.Z, PointB.Z);
    float BottomZ = MeshBounds.Min.Z - 50.0f;

    // [수정] 깊이 확장을 메쉬 크기 비례로 변경
    float DepthExtension = MeshBounds.GetSize().GetMax() * 0.5f + 100.0f;

    if (MinDist == DistMinX || MinDist == DistMaxX)
    {
        MinVec.X = MeshBounds.Min.X - DepthExtension;
        MaxVec.X = MeshBounds.Max.X + DepthExtension;
        MinVec.Y = FMath::Min(PointA.Y, PointB.Y);
        MaxVec.Y = FMath::Max(PointA.Y, PointB.Y);
        MinVec.Z = BottomZ;
        MaxVec.Z = UserDrawHeight;
    }
    else
    {
        MinVec.Y = MeshBounds.Min.Y - DepthExtension;
        MaxVec.Y = MeshBounds.Max.Y + DepthExtension;
        MinVec.X = FMath::Min(PointA.X, PointB.X);
        MaxVec.X = FMath::Max(PointA.X, PointB.X);
        MinVec.Z = BottomZ;
        MaxVec.Z = UserDrawHeight;
    }

    CurrentPreviewBox = FBox(MinVec, MaxVec);

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

    // -------------------------------------------------------------------------
    // [핵심 수정] 서버/클라이언트 분기 처리
    // -------------------------------------------------------------------------
    if (bIsValidCut)
    {
        if (GetOwner()->HasAuthority())
        {
            // 서버: 직접 생성
            Internal_CreateWeakSpot(CurrentPreviewBox);
        }
        else
        {
            // 클라이언트: 서버에 요청 (RPC)
            Server_RequestCreateWeakSpot(CurrentPreviewBox.Min, CurrentPreviewBox.Max);
        }
    }
    else
    {
        if (!bHasFirstPoint) { UE_LOG(LogTemp, Warning, TEXT("[MiniGame] 취소: 테두리 미진입")); }
        else UE_LOG(LogTemp, Warning, TEXT("[MiniGame] 취소: 연결 끊김/짧음"));
    }

    bIsMarking = false;
    bHasFirstPoint = false;
}

// -----------------------------------------------------------------------------
// [NEW] 서버 RPC 구현
// -----------------------------------------------------------------------------
void UMDF_MiniGameComponent::Server_RequestCreateWeakSpot_Implementation(FVector BoxMin, FVector BoxMax)
{
    // 서버에서 실행됨 - 클라이언트가 보낸 박스 데이터로 약점 생성
    FBox ReceivedBox(BoxMin, BoxMax);
    
    // 간단한 검증: 박스 크기가 너무 크거나 작으면 거부
    FVector BoxSize = ReceivedBox.GetSize();
    if (BoxSize.GetMin() < 1.0f || BoxSize.GetMax() > 10000.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MiniGame] 서버: 비정상적인 박스 크기 거부"));
        return;
    }

    Internal_CreateWeakSpot(ReceivedBox);
    UE_LOG(LogTemp, Log, TEXT("[MiniGame] 서버: 클라이언트 요청으로 약점 생성"));
}

void UMDF_MiniGameComponent::Internal_CreateWeakSpot(const FBox& LocalBox)
{
    FWeakSpotData NewSpot;
    NewSpot.ID = FGuid::NewGuid();
    NewSpot.LocalBox = LocalBox;
    NewSpot.MaxHP = CalculateHPFromBox(LocalBox);
    NewSpot.CurrentHP = NewSpot.MaxHP;
    NewSpot.bIsBroken = false;
    
    WeakSpots.Add(NewSpot);
    UE_LOG(LogTemp, Display, TEXT("[MiniGame] >> 영역 확정! HP: %.1f"), NewSpot.MaxHP);
}

// -----------------------------------------------------------------------------
// [파괴 로직]
// -----------------------------------------------------------------------------
bool UMDF_MiniGameComponent::TryBreach(const FHitResult& HitInfo, float DamageAmount)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return false;
    FVector LocalHit = GetLocalLocationFromWorld(HitInfo.Location);

    for (int32 i = 0; i < WeakSpots.Num(); ++i)
    {
        if (WeakSpots[i].bIsBroken) continue;

        if (WeakSpots[i].LocalBox.ExpandBy(5.0f).IsInside(LocalHit))
        {
            WeakSpots[i].CurrentHP -= DamageAmount;
            UE_LOG(LogTemp, Display, TEXT("   >>> [HIT!] 약점 명중! (Index: %d, 남은HP: %.1f)"), i, WeakSpots[i].CurrentHP);

            if (WeakSpots[i].CurrentHP <= 0.0f)
            {
                UE_LOG(LogTemp, Error, TEXT("   >>> [DESTROY] 파괴 조건 달성! 절단 실행!"));
                ExecuteDestruction(i);
            }
            return true;
        }
    }
    return false;
}

void UMDF_MiniGameComponent::ExecuteDestruction(int32 WeakSpotIndex)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (!WeakSpots.IsValidIndex(WeakSpotIndex) || WeakSpots[WeakSpotIndex].bIsBroken) return; 

    WeakSpots[WeakSpotIndex].bIsBroken = true;
    ApplyVisualMeshCut(WeakSpotIndex);
}

void UMDF_MiniGameComponent::ApplyVisualMeshCut(int32 Index)
{
    if (!WeakSpots.IsValidIndex(Index)) return;
    if (LocallyProcessedIndices.Contains(Index)) return;
    LocallyProcessedIndices.Add(Index);

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp || !DynComp->GetDynamicMesh()) return;

    UDynamicMesh* TargetMesh = DynComp->GetDynamicMesh();

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

    FGeometryScriptMeshBooleanOptions BoolOptions;
    BoolOptions.bFillHoles = true;       
    BoolOptions.bSimplifyOutput = false; 

    UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
        TargetMesh, FTransform::Identity, ToolMesh, FTransform::Identity, 
        EGeometryScriptBooleanOperation::Subtract, BoolOptions
    );
    
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(TargetMesh, FGeometryScriptCalculateNormalsOptions());
    
    FBox MeshBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(TargetMesh);
    FTransform BoxTransform = FTransform::Identity;
    BoxTransform.SetTranslation(MeshBounds.GetCenter());
    BoxTransform.SetScale3D(MeshBounds.GetSize());

    UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(TargetMesh, 0, BoxTransform, FGeometryScriptMeshSelection());
    UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents(TargetMesh, FGeometryScriptTangentsOptions());
    
    DynComp->MarkRenderTransformDirty(); 
    DynComp->NotifyMeshUpdated();        
    DynComp->UpdateCollision(true);      
    DynComp->MarkRenderStateDirty();

    // [메모리 정리] ToolMesh 명시적 마킹
    if (ToolMesh)
    {
        ToolMesh->MarkAsGarbage();
    }

    UE_LOG(LogTemp, Log, TEXT("[MDF Sync] 로컬 절단 시각화 연산 완료 (Index: %d)"), Index);
}

// -----------------------------------------------------------------------------
// [유틸리티]
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
    
    // [수정] 스케일 이중 적용 제거 - LocalBox는 이미 로컬 좌표
    return 100.0f + (LocalVolume * HPDensityMultiplier * 0.005f);
}

void UMDF_MiniGameComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UDynamicMeshComponent* DynComp = GetOwner() ? GetOwner()->FindComponentByClass<UDynamicMeshComponent>() : nullptr;
    if (!DynComp) return;

    FTransform CompTrans = DynComp->GetComponentTransform();
    FVector WorldScale = CompTrans.GetScale3D();
    
    FBox WallBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(DynComp->GetDynamicMesh());

    for (const FWeakSpotData& Spot : WeakSpots)
    {
        if (Spot.bIsBroken) continue;
        FBox VisualBox = Spot.LocalBox.Overlap(WallBounds).ExpandBy(0.5f);
        FVector ScaledExtent = VisualBox.GetExtent() * WorldScale;
        DrawDebugBox(GetWorld(), CompTrans.TransformPosition(VisualBox.GetCenter()), ScaledExtent, CompTrans.GetRotation(), FColor::Cyan, false, -1.0f, 0, 1.5f);
    }

    if (bIsMarking)
    {
        FColor DrawColor = bIsValidCut ? FColor::Green : FColor::Red;
        FBox VisualPreview = CurrentPreviewBox.Overlap(WallBounds).ExpandBy(0.5f);
        FVector ScaledExtent = VisualPreview.GetExtent() * WorldScale;
        DrawDebugBox(GetWorld(), CompTrans.TransformPosition(VisualPreview.GetCenter()), ScaledExtent, CompTrans.GetRotation(), DrawColor, false, -1.0f, 0, 3.0f);
    }
}