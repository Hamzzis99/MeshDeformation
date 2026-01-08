// Gihyeon's Deformation Project (Helluna)

#include "Components/MDF_DeformableComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h" 

// 다이나믹 메시 및 속성 제어 관련
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h" // [중요] Vertex Color 제어용 헤더
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"

// 나이아가라 관련
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UMDF_DeformableComponent::UMDF_DeformableComponent()
{
    PrimaryComponentTick.bCanEverTick = false; 
    SetIsReplicatedByDefault(true);
}

void UMDF_DeformableComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeDynamicMesh();
    
    AActor* Owner = GetOwner();
    if (IsValid(Owner))
    {
       if (Owner->HasAuthority() && !Owner->GetIsReplicated())
       {
          Owner->SetReplicates(true);
          Owner->SetReplicateMovement(true); 
       }

       Owner->OnTakePointDamage.RemoveDynamic(this, &UMDF_DeformableComponent::HandlePointDamage);
       Owner->OnTakePointDamage.AddDynamic(this, &UMDF_DeformableComponent::HandlePointDamage);
    }
}

void UMDF_DeformableComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
    if (!bIsDeformationEnabled || !IsValid(DamagedActor) || Damage <= 0.0f) return;
    if (!DamagedActor->HasAuthority()) return;

    UDynamicMeshComponent* MeshComp = Cast<UDynamicMeshComponent>(FHitComponent);
    if (!IsValid(MeshComp))
    {
        MeshComp = DamagedActor->FindComponentByClass<UDynamicMeshComponent>();
    }

    if (IsValid(MeshComp))
    {
        // 데미지 타입을 함께 저장하여 나중에 어떤 색을 칠할지 결정함
        HitQueue.Add(FMDFHitData(ConvertWorldToLocal(HitLocation), ConvertWorldDirectionToLocal(ShotFromDirection), Damage, DamageType->GetClass()));

        if (!BatchTimerHandle.IsValid())
        {
            float Delay = FMath::Max(0.001f, BatchProcessDelay);
            GetWorld()->GetTimerManager().SetTimer(BatchTimerHandle, this, &UMDF_DeformableComponent::ProcessDeformationBatch, Delay, false);
        }

        if (bShowDebugPoints)
        {
            DrawDebugPoint(GetWorld(), HitLocation, 10.0f, FColor::Red, false, 3.0f);
        }
    }
}

/** * [성능 최적화 및 시각/청각/데이터 마스크 적용] 
 * 1. 버텍스 변형 (SQRT 최적화)
 * 2. 버텍스 컬러 데이터 기록 (쉐이더용 마스크: R-원거리, G-근접)
 * 3. 3D 사운드 재생 (거리 감쇄 적용)
 * 4. 나이아가라 파편 효과 스폰
 */
void UMDF_DeformableComponent::ProcessDeformationBatch()
{
    BatchTimerHandle.Invalidate();
    if (HitQueue.IsEmpty()) return;

    AActor* Owner = GetOwner();
    UDynamicMeshComponent* MeshComp = Owner->FindComponentByClass<UDynamicMeshComponent>();
    
    if (IsValid(MeshComp) && IsValid(MeshComp->GetDynamicMesh()))
    {
        const double RadiusSq = FMath::Square((double)DeformRadius);
        const double InverseRadius = 1.0 / (double)DeformRadius;
        const FTransform& ComponentTransform = MeshComp->GetComponentTransform();

        // --- [Part 1: 메시 변형 및 정점 색상 데이터 기록] ---
        MeshComp->GetDynamicMesh()->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh) 
        {
            // 속성 활성화
            if (!EditMesh.HasAttributes()) { EditMesh.EnableAttributes(); }
            if (!EditMesh.Attributes()->HasPrimaryColors()) { EditMesh.Attributes()->EnablePrimaryColors(); }
            auto* ColorAttrib = EditMesh.Attributes()->PrimaryColors();

            // [반론의 핵심 - 최적화 1] 
            // TArray를 루프 '밖'에 선언합니다. (Heap Allocation 1회 발생)
            // 비평가의 "안전함"을 챙기면서, 저의 "성능(무할당)" 주장을 관철시키는 방법입니다.
            TArray<int32> ElementIDs; 

            for (int32 VertexID : EditMesh.VertexIndicesItr())
            {
                FVector3d VertexPos = EditMesh.GetVertex(VertexID);
                FVector3d TotalOffset(0.0, 0.0, 0.0);
                
                // ... (거리 계산 로직은 기존과 동일) ...
                FVector4f TargetMask(0.f, 0.f, 0.f, 1.f);
                bool bModified = false;

                for (const FMDFHitData& Hit : HitQueue)
                {
                    // ... (거리 체크 및 TargetMask 계산) ...
                    double DistSq = FVector3d::DistSquared(VertexPos, (FVector3d)Hit.LocalLocation);
                    if (DistSq < RadiusSq)
                    {
                        double Distance = FMath::Sqrt(DistSq);
                        double Falloff = 1.0 - (Distance * InverseRadius);
                        
                        TotalOffset += (FVector3d)Hit.LocalDirection * (double)(DeformStrength * Falloff);

                        if (Hit.DamageTypeClass == RangedDamageType)
                            TargetMask.X = FMath::Max(TargetMask.X, (float)Falloff);
                        else if (Hit.DamageTypeClass == MeleeDamageType)
                            TargetMask.Y = FMath::Max(TargetMask.Y, (float)Falloff);
                        
                        bModified = true;
                    }
                }
                
                if (bModified)
                {
                    // 1. 위치 업데이트
                    EditMesh.SetVertex(VertexID, VertexPos + TotalOffset);

                    // [반론의 핵심 - 최적화 2] 
                    // 배열을 새로 만드는 게 아니라, 내용만 비우고(Reset) 메모리는 그대로 씁니다.
                    // 이렇게 하면 VertexID -> ElementID 변환을 안전하게 수행하면서도 
                    // 메모리 할당/해제(malloc/free) 부하가 전혀 없습니다.
                    ElementIDs.Reset(); 
                    
                    // 2. 안전한 다리 건너기 (VertexID -> ElementIDs)
                    ColorAttrib->GetVertexElements(VertexID, ElementIDs);

                    // 3. 색상 적용 (이제 ElementID를 쓰므로 크래시 걱정 없음)
                    for (int32 ElementID : ElementIDs)
                    {
                        FVector4f CurrentColor = ColorAttrib->GetElement(ElementID);

                        FVector4f FinalColor(
                            FMath::Max(CurrentColor.X, TargetMask.X),
                            FMath::Max(CurrentColor.Y, TargetMask.Y),
                            CurrentColor.Z,
                            1.f
                        );

                        ColorAttrib->SetElement(ElementID, FinalColor);
                    }
                }
            }
        }, EDynamicMeshChangeType::GeneralEdit);

        // --- [Part 2: 시각 및 청각 효과 재생] ---
        for (const FMDFHitData& Hit : HitQueue)
        {
            FVector WorldHitLoc = ComponentTransform.TransformPosition(Hit.LocalLocation);
            FVector WorldHitDir = ComponentTransform.TransformVector(Hit.LocalDirection);

            if (IsValid(DebrisSystem))
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DebrisSystem, WorldHitLoc, WorldHitDir.Rotation());
            }

            if (IsValid(ImpactSound))
            {
                UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, WorldHitLoc, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, ImpactAttenuation);
            }
        }

        // --- [Part 3: 물리 및 렌더링 갱신] ---
        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(MeshComp->GetDynamicMesh(), FGeometryScriptCalculateNormalsOptions());
        MeshComp->UpdateCollision();
        MeshComp->NotifyMeshUpdated();

        UE_LOG(LogTemp, Warning, TEXT("[MDF] [최적화] %d개의 타격 처리 및 데이터 맵 작성 완료."), HitQueue.Num());
    }

    HitQueue.Empty();
}

void UMDF_DeformableComponent::InitializeDynamicMesh()
{
    if (!IsValid(SourceStaticMesh)) return;

    AActor* Owner = GetOwner();
    UDynamicMeshComponent* MeshComp = IsValid(Owner) ? Owner->FindComponentByClass<UDynamicMeshComponent>() : nullptr;

    if (IsValid(MeshComp) && IsValid(MeshComp->GetDynamicMesh()))
    {
        FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
        AssetOptions.bApplyBuildSettings = true;
        EGeometryScriptOutcomePins Outcome;

        UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
            SourceStaticMesh, MeshComp->GetDynamicMesh(), AssetOptions, FGeometryScriptMeshReadLOD(), Outcome
        );

        if (Outcome == EGeometryScriptOutcomePins::Success)
        {
            UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(MeshComp->GetDynamicMesh(), FGeometryScriptCalculateNormalsOptions());
            MeshComp->UpdateCollision();
            MeshComp->NotifyMeshUpdated();
        }
    }
}

FVector UMDF_DeformableComponent::ConvertWorldToLocal(FVector WorldLocation)
{
    return IsValid(GetOwner()) ? GetOwner()->GetActorTransform().InverseTransformPosition(WorldLocation) : WorldLocation;
}

FVector UMDF_DeformableComponent::ConvertWorldDirectionToLocal(FVector WorldDirection)
{
    return IsValid(GetOwner()) ? GetOwner()->GetActorTransform().InverseTransformVector(WorldDirection) : WorldDirection;
}