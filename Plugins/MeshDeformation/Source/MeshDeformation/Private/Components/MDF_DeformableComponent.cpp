#include "Components/MDF_DeformableComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

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
          UE_LOG(LogTemp, Warning, TEXT("[MDF] [설정 변경] %s 액터의 복제를 강제로 활성화했습니다."), *Owner->GetName());
       }

       Owner->OnTakePointDamage.AddDynamic(this, &UMDF_DeformableComponent::HandlePointDamage);
       UE_LOG(LogTemp, Log, TEXT("[MDF] [성공] %s 액터에 MDF 컴포넌트 부착됨."), *Owner->GetName());
    }
}

void UMDF_DeformableComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
    // [Safety High] 권한 및 시스템 활성화 체크
    if (!bIsDeformationEnabled || !IsValid(DamagedActor) || Damage <= 0.0f) return;

    // 서버 권한이 있는지 확인 (데디케이티드 서버 로직)
    if (!DamagedActor->HasAuthority()) return;

    // 타격된 컴포넌트가 DynamicMesh인지 우선 확인 (순서 최적화)
    UDynamicMeshComponent* MeshComp = Cast<UDynamicMeshComponent>(FHitComponent);
    
    // 만약 직접 Cast 실패 시 액터 내에서 다시 탐색
    if (!IsValid(MeshComp))
    {
        MeshComp = DamagedActor->FindComponentByClass<UDynamicMeshComponent>();
    }

    if (IsValid(MeshComp))
    {
        const FVector LocalHitLocation = ConvertWorldToLocal(HitLocation);
        const FVector LocalDirection = ConvertWorldDirectionToLocal(ShotFromDirection);

        // 변형 실행
        DeformMesh(MeshComp, LocalHitLocation, LocalDirection, Damage);

        // [서버 로그] 성공 시 서버 로그에 확실히 기록 (클라이언트에선 안 보임)
        UE_LOG(LogTemp, Warning, TEXT("[MDF] [서버 변형 성공] 대상: %s / 위치: %s"), 
            *DamagedActor->GetName(), *LocalHitLocation.ToString());

        if (bShowDebugPoints)
        {
            // 서버 뷰포트에서는 빨간 점이 보입니다.
            DrawDebugPoint(GetWorld(), HitLocation, 15.0f, FColor::Red, false, 5.0f);
            
            // 모든 클라이언트 화면에 메시지 출력 (서버에서 호출 시 클라이언트로 전파됨)
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, 
                    FString::Printf(TEXT("[MDF] 서버에서 %s 변형 처리됨!"), *DamagedActor->GetName()));
            }
        }
    }
    else 
    {
        UE_LOG(LogTemp, Error, TEXT("[MDF] 실패: DynamicMeshComponent를 찾을 수 없음!"));
    }
}

void UMDF_DeformableComponent::DeformMesh(UDynamicMeshComponent* MeshComp, const FVector& LocalLocation, const FVector& LocalDirection, float Damage)
{
    if (!IsValid(MeshComp) || !MeshComp->GetDynamicMesh()) return;

    // 사용자님이 제공해주신 UDynamicMesh.h 규격 (UE 5.7.1)
    MeshComp->GetDynamicMesh()->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh) 
    {
        for (int32 VertexID : EditMesh.VertexIndicesItr())
        {
            FVector3d VertexPos = EditMesh.GetVertex(VertexID);
            double Distance = FVector3d::Distance(VertexPos, (FVector3d)LocalLocation);

            if (Distance < (double)DeformRadius)
            {
                double Falloff = 1.0 - (Distance / (double)DeformRadius);
                // 발사 방향으로 점 이동
                FVector3d Offset = (FVector3d)LocalDirection * (double)(DeformStrength * Falloff);
                EditMesh.SetVertex(VertexID, VertexPos + Offset);
            }
        }
    }, EDynamicMeshChangeType::DeformationEdit); //

    // 노멀(법선)과 탄젠트를 다시 계산해야 찌그러진 부위의 음영이 정확히 보입니다.
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(MeshComp->GetDynamicMesh(), FGeometryScriptCalculateNormalsOptions());
    
    // 렌더링 및 물리 데이터 갱신 (클라이언트로 동기화 트리거)
    MeshComp->NotifyMeshUpdated();
}

void UMDF_DeformableComponent::InitializeDynamicMesh()
{
    // 1. 소스 메시 유효성 검사
    if (!IsValid(SourceStaticMesh)) return;

    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) return;

    // 2. 부모 액터에서 다이나믹 메시 컴포넌트 찾기
    UDynamicMeshComponent* MeshComp = Owner->FindComponentByClass<UDynamicMeshComponent>();

    if (IsValid(MeshComp) && IsValid(MeshComp->GetDynamicMesh()))
    {
        FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
        AssetOptions.bApplyBuildSettings = true;

        FGeometryScriptMeshReadLOD RequestedLOD;
        RequestedLOD.LODIndex = 0;

        EGeometryScriptOutcomePins Outcome;

        // 3. 에셋으로부터 메시 데이터 복사
        UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
            SourceStaticMesh, 
            MeshComp->GetDynamicMesh(), 
            AssetOptions, 
            RequestedLOD, 
            Outcome
        );

        if (Outcome == EGeometryScriptOutcomePins::Success)
        {
            // [추가] 4. 노멀 및 탄젠트 재계산 (찌그러짐이 입체적으로 보이게 함)
            // 에디터 프리뷰와 게임 런타임 모두에서 깔끔한 음영을 보장합니다.
            FGeometryScriptCalculateNormalsOptions NormalOptions;
            UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(MeshComp->GetDynamicMesh(), NormalOptions);

            // 5. 변경 사항 알림 (렌더링 및 물리 데이터 동기화)
            MeshComp->NotifyMeshUpdated();
            
            UE_LOG(LogTemp, Log, TEXT("[MDF] %s 프리뷰 메시 초기화 성공 (Actor: %s)"), 
                *SourceStaticMesh->GetName(), *Owner->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[MDF] 메시 복사 실패"));
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