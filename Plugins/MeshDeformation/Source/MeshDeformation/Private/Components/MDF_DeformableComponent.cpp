#include "Components/MDF_DeformableComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"

UMDF_DeformableComponent::UMDF_DeformableComponent()
{
    PrimaryComponentTick.bCanEverTick = false; 
    SetIsReplicatedByDefault(true);
}

void UMDF_DeformableComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (IsValid(Owner))
    {
       if (Owner->HasAuthority() && !Owner->GetIsReplicated())
       {
          Owner->SetReplicates(true);
          Owner->SetReplicateMovement(true); 
          UE_LOG(LogTemp, Warning, TEXT("[MeshDeformation] [설정 변경] %s 액터의 복제를 강제로 활성화했습니다."), *Owner->GetName());
       }

       Owner->OnTakePointDamage.AddDynamic(this, &UMDF_DeformableComponent::HandlePointDamage);
       UE_LOG(LogTemp, Log, TEXT("[MeshDeformation] [성공] %s 액터에 MDF 컴포넌트 부착됨."), *Owner->GetName());
    }
}

void UMDF_DeformableComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
    // High Safety: 서버 권한 검사 필수
    if (!bIsDeformationEnabled || !IsValid(DamagedActor) || !DamagedActor->HasAuthority() || Damage <= 0.0f) return;

    UDynamicMeshComponent* MeshComp = DamagedActor->FindComponentByClass<UDynamicMeshComponent>();
    if (IsValid(MeshComp))
    {
        const FVector LocalHitLocation = ConvertWorldToLocal(HitLocation);
        const FVector LocalDirection = ConvertWorldDirectionToLocal(ShotFromDirection);

        DeformMesh(MeshComp, LocalHitLocation, LocalDirection, Damage);

        if (bShowDebugPoints)
        {
            DrawDebugPoint(GetWorld(), HitLocation, 10.0f, FColor::Red, false, 3.0f);
        }
    }
}

void UMDF_DeformableComponent::DeformMesh(UDynamicMeshComponent* MeshComp, const FVector& LocalLocation, const FVector& LocalDirection, float Damage)
{
    if (!IsValid(MeshComp) || !MeshComp->GetDynamicMesh()) return;

    // [수정] 사용자님의 UDynamicMesh.h 규격에 맞춰 EDynamicMeshChangeType을 사용합니다.
    MeshComp->GetDynamicMesh()->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh) 
    {
        for (int32 VertexID : EditMesh.VertexIndicesItr())
        {
            FVector3d VertexPos = EditMesh.GetVertex(VertexID);
            double Distance = FVector3d::Distance(VertexPos, (FVector3d)LocalLocation);

            if (Distance < (double)DeformRadius)
            {
                double Falloff = 1.0 - (Distance / (double)DeformRadius);
                FVector3d Offset = (FVector3d)LocalDirection * (double)(DeformStrength * Falloff);
                EditMesh.SetVertex(VertexID, VertexPos + Offset);
            }
        }
    }, EDynamicMeshChangeType::DeformationEdit); // [핵심 수정 부분]

    MeshComp->NotifyMeshUpdated();
}

FVector UMDF_DeformableComponent::ConvertWorldToLocal(FVector WorldLocation)
{
    return IsValid(GetOwner()) ? GetOwner()->GetActorTransform().InverseTransformPosition(WorldLocation) : WorldLocation;
}

FVector UMDF_DeformableComponent::ConvertWorldDirectionToLocal(FVector WorldDirection)
{
    return IsValid(GetOwner()) ? GetOwner()->GetActorTransform().InverseTransformVector(WorldDirection) : WorldDirection;
}