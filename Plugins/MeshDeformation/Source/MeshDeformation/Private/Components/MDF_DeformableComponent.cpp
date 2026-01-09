// Gihyeon's Deformation Project (Helluna)

#include "Components/MDF_DeformableComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h" 

// 다이나믹 메시 관련 헤더
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
// #include "DynamicMesh/DynamicMeshAttributeSet.h" // 머터리얼(색상) 제어용 헤더는 삭제함
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"

// 나이아가라 관련 헤더
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
    // 1. 기본 유효성 검사
    if (!bIsDeformationEnabled || !IsValid(DamagedActor) || Damage <= 0.0f) return;
    if (!DamagedActor->HasAuthority()) return;

    // --- [안전장치 & 디버깅 권한 검사] 시작 ---
    // 공격자(Attacker) 식별: 컨트롤러가 있으면 Pawn을, 없으면 가해자 액터 자체를 사용
    AActor* Attacker = DamageCauser;
    if (InstigatedBy && InstigatedBy->GetPawn())
    {
        Attacker = InstigatedBy->GetPawn();
    }

    if (IsValid(Attacker))
    {
        // 1. 적군인가? (Enemy 태그 확인 - AI용)
        bool bIsEnemy = Attacker->ActorHasTag(TEXT("Enemy"));
        
        // 2. 테스트 권한이 있는 플레이어인가? (MDF_Test 태그 확인 - 디버깅용)
        bool bIsTester = Attacker->ActorHasTag(TEXT("MDF_Test"));

        // 둘 다 아니라면(아군이거나 권한 없는 대상) 찌그러트리지 않고 무시함
        if (!bIsEnemy && !bIsTester)
        {
            return; 
        }
    }
    // --- [안전장치 & 디버깅 권한 검사] 끝 ---


    // 2. 실제 변형 로직 진행
    // 디버그 로그: 어떤 데미지 타입이 들어왔는지 확인
    FString DmgTypeName = IsValid(DamageType) ? DamageType->GetName() : TEXT("None");
    UE_LOG(LogTemp, Warning, TEXT("[MDF] [수신] 데미지 감지! 데미지: %.1f / 타입: %s / 공격자: %s"), Damage, *DmgTypeName, *GetNameSafe(Attacker));

    UDynamicMeshComponent* MeshComp = Cast<UDynamicMeshComponent>(FHitComponent);
    if (!IsValid(MeshComp))
    {
        MeshComp = DamagedActor->FindComponentByClass<UDynamicMeshComponent>();
    }

    if (IsValid(MeshComp))
    {
        // 데미지 타입 클래스 정보를 함께 큐에 저장
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

/** * [최적화된 배칭 처리 함수]
 * 1. 머터리얼(색상) 로직 삭제됨 -> 오직 물리적 변형(위치 이동)만 수행
 * 2. 데미지 타입에 따라 변형 강도(깊이) 조절
 * 3. 나이아가라 이펙트 및 사운드 재생 유지
 */
void UMDF_DeformableComponent::ProcessDeformationBatch()
{
    BatchTimerHandle.Invalidate();
    if (HitQueue.IsEmpty()) return;

    AActor* Owner = GetOwner();
    UDynamicMeshComponent* MeshComp = Owner->FindComponentByClass<UDynamicMeshComponent>();
    
    if (IsValid(MeshComp) && IsValid(MeshComp->GetDynamicMesh()))
    {
        // [반지름 최적화] 매번 계산하지 않고 미리 제곱값과 역수를 구해둡니다.
        const double RadiusSq = FMath::Square((double)DeformRadius);
        const double InverseRadius = 1.0 / (double)DeformRadius;
        
        const FTransform& ComponentTransform = MeshComp->GetComponentTransform();

        // --- [Part 1: 순수 메시 변형 (Deformation Only)] ---
        MeshComp->GetDynamicMesh()->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh) 
        {
            // Vertex Color 관련 초기화 코드 삭제됨

            for (int32 VertexID : EditMesh.VertexIndicesItr())
            {
                FVector3d VertexPos = EditMesh.GetVertex(VertexID);
                FVector3d TotalOffset(0.0, 0.0, 0.0);
                
                bool bModified = false;

                for (const FMDFHitData& Hit : HitQueue)
                {
                    double DistSq = FVector3d::DistSquared(VertexPos, (FVector3d)Hit.LocalLocation);
                    
                    // 범위 안에 있는 버텍스만 계산
                    if (DistSq < RadiusSq)
                    {
                        // 1. 거리 감쇄 (중앙은 1.0, 끝은 0.0)
                        double Distance = FMath::Sqrt(DistSq);
                        double Falloff = 1.0 - (Distance * InverseRadius);
                        
                        // [핵심 변경] 데미지를 강도에 반영!
                        // DeformStrength: 에디터 설정 기본값
                        // Hit.Damage: 실제 총알 데미지
                        // 0.05f: 데미지가 100일 때 너무 푹 꺼지지 않도록 조절하는 스케일 값 (취향껏 조절)
                        float DamageFactor = Hit.Damage * 0.05f; 
                        float CurrentStrength = DeformStrength * DamageFactor;

                        // 데미지 타입별 추가 보정 (옵션)
                        if (Hit.DamageTypeClass && MeleeDamageType && Hit.DamageTypeClass->IsChildOf(MeleeDamageType)) 
                        {
                            // 근접 공격은 둔탁하니까 1.5배 더 깊게
                            CurrentStrength *= 1.5f; 
                        }
                        // 원거리 공격은 데미지가 낮으면 알아서 얕게 파임 (추가 보정 불필요 시 생략 가능)
                        else if (Hit.DamageTypeClass && RangedDamageType && Hit.DamageTypeClass->IsChildOf(RangedDamageType))
                        {
                            CurrentStrength *= 0.5f; 
                        }

                        // 변형 적용 (방향 * 강도 * 거리감쇄)
                        TotalOffset += (FVector3d)Hit.LocalDirection * (double)(CurrentStrength * Falloff);
                        bModified = true;
                    }
                }
                
                if (bModified)
                {
                    // 위치만 업데이트 (색상 관련 함수 호출 삭제됨)
                    EditMesh.SetVertex(VertexID, VertexPos + TotalOffset);
                }
            }
        }, EDynamicMeshChangeType::GeneralEdit);

        // --- [Part 2: 시각(Niagara) 및 청각(Sound) 효과 재생] ---
        for (const FMDFHitData& Hit : HitQueue)
        {
            FVector WorldHitLoc = ComponentTransform.TransformPosition(Hit.LocalLocation);
            FVector WorldHitDir = ComponentTransform.TransformVector(Hit.LocalDirection);

            // 나이아가라 파편 효과
            if (IsValid(DebrisSystem))
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DebrisSystem, WorldHitLoc, WorldHitDir.Rotation());
            }

            // 타격 사운드
            if (IsValid(ImpactSound))
            {
                UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSound, WorldHitLoc, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, ImpactAttenuation);
            }
        }

        // --- [Part 3: 물리 및 렌더링 갱신] ---
        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(MeshComp->GetDynamicMesh(), FGeometryScriptCalculateNormalsOptions());
        MeshComp->UpdateCollision();
        MeshComp->NotifyMeshUpdated();

        UE_LOG(LogTemp, Warning, TEXT("[MDF] [최적화] %d개의 타격 처리 완료 (데미지 반영됨)."), HitQueue.Num());
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