// Gihyeon's Deformation Project (Helluna)

#include "Components/MDF_DeformableComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h" // 사운드 재생용

// 다이나믹 메시 관련
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
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
        HitQueue.Add(FMDFHitData(ConvertWorldToLocal(HitLocation), ConvertWorldDirectionToLocal(ShotFromDirection), Damage));

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

/** * [성능 최적화 및 시각/청각 효과 적용] 
 * 1. 버텍스 변형 (SQRT 최적화 적용) 
 * 2. 3D 사운드 재생 
 * 3. 나이아가라 파편 스폰
 */
void UMDF_DeformableComponent::ProcessDeformationBatch()
{
    BatchTimerHandle.Invalidate();
    if (HitQueue.IsEmpty()) return;

    AActor* Owner = GetOwner();
    UDynamicMeshComponent* MeshComp = Owner->FindComponentByClass<UDynamicMeshComponent>();
    
    if (IsValid(MeshComp) && IsValid(MeshComp->GetDynamicMesh()))
    {
        // 최적화: 루프 외부 캐싱
        const double RadiusSq = FMath::Square((double)DeformRadius);
        const double InverseRadius = 1.0 / (double)DeformRadius;
        const FTransform& ComponentTransform = MeshComp->GetComponentTransform();

        // --- [Part 1: 메시 변형 연산] ---
        MeshComp->GetDynamicMesh()->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh) 
        {
            for (int32 VertexID : EditMesh.VertexIndicesItr())
            {
                FVector3d VertexPos = EditMesh.GetVertex(VertexID);
                FVector3d TotalOffset(0.0, 0.0, 0.0);
                bool bModified = false;

                for (const FMDFHitData& Hit : HitQueue)
                {
                    double DistSq = FVector3d::DistSquared(VertexPos, (FVector3d)Hit.LocalLocation);
                    if (DistSq < RadiusSq)
                    {
                        double Distance = FMath::Sqrt(DistSq);
                        double Falloff = 1.0 - (Distance * InverseRadius);
                        TotalOffset += (FVector3d)Hit.LocalDirection * (double)(DeformStrength * Falloff);
                        bModified = true;
                    }
                }

                if (bModified)
                {
                    EditMesh.SetVertex(VertexID, VertexPos + TotalOffset);
                }
            }
        }, EDynamicMeshChangeType::GeneralEdit);

        // --- [Part 2: 시각 및 청각 효과 재생] ---
        for (const FMDFHitData& Hit : HitQueue)
        {
            FVector WorldHitLoc = ComponentTransform.TransformPosition(Hit.LocalLocation);
            FVector WorldHitDir = ComponentTransform.TransformVector(Hit.LocalDirection);

            // 1. 나이아가라 파편 스폰
            if (IsValid(DebrisSystem))
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DebrisSystem, WorldHitLoc, WorldHitDir.Rotation());
            }

            // 2. 3D 피격 사운드 재생 (기현님이 BP에서 설정한 Attenuation 적용)
            if (IsValid(ImpactSound))
            {
                UGameplayStatics::PlaySoundAtLocation(
                    GetWorld(), 
                    ImpactSound, 
                    WorldHitLoc, 
                    FRotator::ZeroRotator, 
                    1.0f, // 볼륨
                    1.0f, // 피치
                    0.0f, // 시작 시간
                    ImpactAttenuation // [핵심] 블루프린트에서 고른 감쇄 설정 적용
                );
            }
        }

        // --- [Part 3: 물리 및 렌더링 갱신] ---
        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(MeshComp->GetDynamicMesh(), FGeometryScriptCalculateNormalsOptions());
        MeshComp->UpdateCollision();
        MeshComp->NotifyMeshUpdated();

        UE_LOG(LogTemp, Warning, TEXT("[MDF] [최적화] %d개의 타격 처리 완료 (3D 사운드 감쇄 적용)"), HitQueue.Num());
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