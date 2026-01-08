// Gihyeon's Deformation Project (Helluna)

#include "Components/MDF_DeformableComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"

#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"

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

void UMDF_DeformableComponent::ProcessDeformationBatch()
{
    BatchTimerHandle.Invalidate();
    if (HitQueue.IsEmpty()) return;

    AActor* Owner = GetOwner();
    UDynamicMeshComponent* MeshComp = Owner->FindComponentByClass<UDynamicMeshComponent>();
    
    if (IsValid(MeshComp) && IsValid(MeshComp->GetDynamicMesh()))
    {
        // [수정] EDynamicMeshChangeType::GeneralEdit을 사용하여 내부 ensure 에러 방지
        MeshComp->GetDynamicMesh()->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh) 
        {
            for (int32 VertexID : EditMesh.VertexIndicesItr())
            {
                FVector3d VertexPos = EditMesh.GetVertex(VertexID);
                FVector3d TotalOffset(0.0, 0.0, 0.0);

                for (const FMDFHitData& Hit : HitQueue)
                {
                    double Distance = FVector3d::Distance(VertexPos, (FVector3d)Hit.LocalLocation);
                    if (Distance < (double)DeformRadius)
                    {
                        double Falloff = 1.0 - (Distance / (double)DeformRadius);
                        TotalOffset += (FVector3d)Hit.LocalDirection * (double)(DeformStrength * Falloff);
                    }
                }

                if (!TotalOffset.IsZero())
                {
                    EditMesh.SetVertex(VertexID, VertexPos + TotalOffset);
                }
            }
        }, EDynamicMeshChangeType::GeneralEdit); // 변경 타입을 GeneralEdit으로 조정

        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(MeshComp->GetDynamicMesh(), FGeometryScriptCalculateNormalsOptions());
        
        // [수정] 인자 없이 호출 (UE 5.x 표준 API)
        MeshComp->NotifyMeshUpdated();

        UE_LOG(LogTemp, Warning, TEXT("[MDF] [최적화] %d개의 타격을 한 번에 처리했습니다."), HitQueue.Num());
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