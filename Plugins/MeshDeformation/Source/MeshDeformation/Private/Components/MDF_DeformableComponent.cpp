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
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"

// 나이아가라 관련 헤더
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UMDF_DeformableComponent::UMDF_DeformableComponent()
{
    PrimaryComponentTick.bCanEverTick = false; 
    SetIsReplicatedByDefault(true); // [Step 7] 컴포넌트 복제 활성화
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

       // 데미지 델리게이트 등록 (서버/클라 모두 등록하되 처리는 HandlePointDamage 내부에서 Authority 체크)
       Owner->OnTakePointDamage.RemoveDynamic(this, &UMDF_DeformableComponent::HandlePointDamage);
       Owner->OnTakePointDamage.AddDynamic(this, &UMDF_DeformableComponent::HandlePointDamage);
    }
}

void UMDF_DeformableComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
    // 1. [Server Only] 데미지 처리는 오직 서버 권한이 있는 경우에만 수행
    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority()) return;

    // 2. 기본 유효성 검사
    if (!bIsDeformationEnabled || Damage <= 0.0f) return;

    // --- [안전장치 & 디버깅 권한 검사] (기존 코드의 로직 복원) ---
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

        // 둘 다 아니라면 찌그러트리지 않고 무시함
        if (!bIsEnemy && !bIsTester) return; 
    }
    // -----------------------------------------------------------

    // [Log: 데미지 수신 로그 복원]
    FString DmgTypeName = IsValid(DamageType) ? DamageType->GetName() : TEXT("None");
    UE_LOG(LogTemp, Warning, TEXT("[MDF] [Server 수신] 데미지 감지! 데미지: %.1f / 타입: %s / 공격자: %s"), Damage, *DmgTypeName, *GetNameSafe(Attacker));

    // 3. 큐에 데이터 적재 (서버 메모리)
    UDynamicMeshComponent* MeshComp = Cast<UDynamicMeshComponent>(FHitComponent);
    if (!IsValid(MeshComp))
    {
        MeshComp = GetOwner()->FindComponentByClass<UDynamicMeshComponent>();
    }

    if (IsValid(MeshComp))
    {
        HitQueue.Add(FMDFHitData(ConvertWorldToLocal(HitLocation), ConvertWorldDirectionToLocal(ShotFromDirection), Damage, DamageType ? DamageType->GetClass() : nullptr));

        // 디버그 포인트 그리기 (서버 화면)
        if (bShowDebugPoints)
        {
            DrawDebugPoint(GetWorld(), HitLocation, 10.0f, FColor::Red, false, 3.0f);
        }

        // 타이머가 없으면 작동 (배칭 시작)
        if (!BatchTimerHandle.IsValid())
        {
            float Delay = FMath::Max(0.001f, BatchProcessDelay);
            GetWorld()->GetTimerManager().SetTimer(BatchTimerHandle, this, &UMDF_DeformableComponent::ProcessDeformationBatch, Delay, false);
        }
    }
}

/** * [Step 7-2] 서버 로직 수정 
 * 직접 변형하지 않고, 모인 데이터를 RPC로 방송(Broadcast)합니다.
 */
void UMDF_DeformableComponent::ProcessDeformationBatch()
{
    // 타이머 핸들 초기화
    BatchTimerHandle.Invalidate();

    // 서버인지 다시 한 번 확인 (안전장치)
    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority()) return;

    if (HitQueue.IsEmpty()) return;

    // [Log: RPC 전송]
    UE_LOG(LogTemp, Log, TEXT("[MDF] [Server 전송] 클라이언트들에게 %d개의 변형 데이터를 브로드캐스팅합니다."), HitQueue.Num());

    // [핵심] RPC 호출: 서버가 "이 데이터로 변형해라"라고 모든 클라이언트(자신 포함)에게 명령
    NetMulticast_ApplyDeformation(HitQueue);

    // 전송 완료했으므로 서버의 대기열 비움
    HitQueue.Empty();
}

/** * [Step 7-3] 클라이언트 실행 로직 (RPC 구현부)
 * 서버와 클라이언트 모두 이 함수가 실행되어 실제 메쉬 변형과 이펙트가 발생합니다.
 */
void UMDF_DeformableComponent::NetMulticast_ApplyDeformation_Implementation(const TArray<FMDFHitData>& BatchedHits)
{
    if (BatchedHits.IsEmpty()) return;

    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) return;

    // [Log: 클라이언트/서버 적용 로그]
    FString NetRole = (Owner->GetLocalRole() == ROLE_Authority) ? TEXT("Server") : TEXT("Client");
    UE_LOG(LogTemp, Log, TEXT("[MDF] [%s 적용] 변형 데이터 %d개 적용 시작"), *NetRole, BatchedHits.Num());

    UDynamicMeshComponent* MeshComp = Owner->FindComponentByClass<UDynamicMeshComponent>();
    
    if (IsValid(MeshComp) && IsValid(MeshComp->GetDynamicMesh()))
    {
        const double RadiusSq = FMath::Square((double)DeformRadius);
        const double InverseRadius = 1.0 / (double)DeformRadius;
        
        const FTransform& ComponentTransform = MeshComp->GetComponentTransform();

        // --- [Part 1: 순수 메시 변형] ---
        // 주의: BatchedHits(RPC로 받은 인자)를 사용해야 합니다.
        MeshComp->GetDynamicMesh()->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh) 
        {
            for (int32 VertexID : EditMesh.VertexIndicesItr())
            {
                FVector3d VertexPos = EditMesh.GetVertex(VertexID);
                FVector3d TotalOffset(0.0, 0.0, 0.0);
                bool bModified = false;

                for (const FMDFHitData& Hit : BatchedHits)
                {
                    double DistSq = FVector3d::DistSquared(VertexPos, (FVector3d)Hit.LocalLocation);
                    
                    if (DistSq < RadiusSq)
                    {
                        double Distance = FMath::Sqrt(DistSq);
                        double Falloff = 1.0 - (Distance * InverseRadius);
                        
                        float DamageFactor = Hit.Damage * 0.05f; 
                        float CurrentStrength = DeformStrength * DamageFactor;

                        if (Hit.DamageTypeClass && MeleeDamageType && Hit.DamageTypeClass->IsChildOf(MeleeDamageType)) 
                        {
                            CurrentStrength *= 1.5f; 
                        }
                        else if (Hit.DamageTypeClass && RangedDamageType && Hit.DamageTypeClass->IsChildOf(RangedDamageType))
                        {
                            CurrentStrength *= 0.5f; 
                        }

                        TotalOffset += (FVector3d)Hit.LocalDirection * (double)(CurrentStrength * Falloff);
                        bModified = true;
                    }
                }
                
                if (bModified)
                {
                    EditMesh.SetVertex(VertexID, VertexPos + TotalOffset);
                }
            }
        }, EDynamicMeshChangeType::GeneralEdit);

        // --- [Part 2: 시각(Niagara) 및 청각(Sound) 효과 재생] ---
        // 이펙트는 로컬에서만 중요하므로, Dedicated Server(헤드리스)에서는 스킵하여 성능을 아낄 수 있습니다.
        if (!IsRunningDedicatedServer()) 
        {
            for (const FMDFHitData& Hit : BatchedHits)
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
        }

        // --- [Part 3: 물리 및 렌더링 갱신] ---
        // 충돌 정보 갱신은 서버(물리 판정)와 클라이언트(IK/Trace 등) 모두 필요합니다.
        UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(MeshComp->GetDynamicMesh(), FGeometryScriptCalculateNormalsOptions());
        MeshComp->UpdateCollision();
        MeshComp->NotifyMeshUpdated();
    }
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
            MeshComp->UpdateCollision(); // 초기화 시 충돌 업데이트
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