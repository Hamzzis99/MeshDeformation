// Gihyeon's Deformation Project (Helluna)

#include "Components/MDF_DeformableComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h" 
#include "Net/UnrealNetwork.h" 
#include "Interface/MDF_GameStateInterface.h"
#include "GameFramework/GameStateBase.h"

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
    SetIsReplicatedByDefault(true); 
}

void UMDF_DeformableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMDF_DeformableComponent, HitHistory);
}

void UMDF_DeformableComponent::BeginPlay()
{
    Super::BeginPlay();
    InitializeDynamicMesh();
    
    // 리셋 안전 장치
    LastAppliedIndex = 0;
    
    AActor* Owner = GetOwner();

    // -------------------------------------------------------------------------
    // [Step 9: 인터페이스를 통한 데이터 복구 (Load)]
    // -------------------------------------------------------------------------
    if (IsValid(Owner) && Owner->HasAuthority())
    {
        if (!ComponentGuid.IsValid())
        {
            FString UniqueName = Owner->GetName();
            FGuid::Parse(UniqueName, ComponentGuid);
            if (!ComponentGuid.IsValid()) ComponentGuid = FGuid::NewGuid();
        }

        AGameStateBase* GS = UGameplayStatics::GetGameState(this);
        IMDF_GameStateInterface* MDF_GS = Cast<IMDF_GameStateInterface>(GS);
        
        if (MDF_GS)
        {
            TArray<FMDFHitData> SavedData;
            if (MDF_GS->LoadMDFData(ComponentGuid, SavedData))
            {
                HitHistory = SavedData;
                UE_LOG(LogTemp, Log, TEXT("[MDF] GameState에서 데이터 복원 성공 (%d hit)"), HitHistory.Num());
            }
        }
    }

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
    
    if (HitHistory.Num() > 0)
    {
        OnRep_HitHistory(); 
    }
}

void UMDF_DeformableComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
    // 1. 기본 검증 (서버만 실행, 데미지 0 이하 무시)
    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority()) return;
    if (!bIsDeformationEnabled || Damage <= 0.0f) return;

    AActor* Attacker = DamageCauser;
    if (InstigatedBy && InstigatedBy->GetPawn())
    {
        Attacker = InstigatedBy->GetPawn();
    }

    // -------------------------------------------------------------------------
    // [보안 Check] 태그 검사 로직 (복구 완료)
    // -------------------------------------------------------------------------
    if (IsValid(Attacker))
    {
        // 1. 자해 방지 (내가 쏜 총에 내가 찌그러지면 안 됨)
        if (Attacker == GetOwner()) return;

        // 2. 권한 검사: 'Enemy' 혹은 'MDF_Test' 태그가 있어야만 찌그러짐
        bool bIsEnemy = Attacker->ActorHasTag(TEXT("Enemy"));
        bool bIsTester = Attacker->ActorHasTag(TEXT("MDF_Test"));

        // 둘 다 없으면 -> "넌 자격이 없다" -> 리턴 (무시)
        if (!bIsEnemy && !bIsTester) 
        {
            // UE_LOG(LogTemp, Verbose, TEXT("[MDF Base] 공격 권한 없음 (태그 부족)"));
            return; 
        }
    }

    // -------------------------------------------------------------------------
    // [이하 변형 로직 실행]
    // -------------------------------------------------------------------------
    FString DmgTypeName = IsValid(DamageType) ? DamageType->GetName() : TEXT("None");
    
    UDynamicMeshComponent* MeshComp = Cast<UDynamicMeshComponent>(FHitComponent);
    if (!IsValid(MeshComp))
    {
        MeshComp = GetOwner()->FindComponentByClass<UDynamicMeshComponent>();
    }

    if (IsValid(MeshComp))
    {
        // [좌표 변환] 메쉬 기준으로 정확하게 변환
        FVector LocalPos = ConvertWorldToLocal(HitLocation);
        
        HitQueue.Add(FMDFHitData(LocalPos, ConvertWorldDirectionToLocal(ShotFromDirection), Damage, DamageType ? DamageType->GetClass() : nullptr));

        if (bShowDebugPoints)
        {
            DrawDebugPoint(GetWorld(), HitLocation, 10.0f, FColor::Red, false, 3.0f);
        }

        if (!BatchTimerHandle.IsValid())
        {
            float Delay = FMath::Max(0.001f, BatchProcessDelay);
            GetWorld()->GetTimerManager().SetTimer(BatchTimerHandle, this, &UMDF_DeformableComponent::ProcessDeformationBatch, Delay, false);
        }
    }
}

void UMDF_DeformableComponent::ProcessDeformationBatch()
{
    BatchTimerHandle.Invalidate();

    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority()) return;
    if (HitQueue.IsEmpty()) return;

    HitHistory.Append(HitQueue);

    // UE_LOG(LogTemp, Log, TEXT("[MDF] [Server 전송] %d개의 변형 발생."), HitQueue.Num());

    if (ComponentGuid.IsValid())
    {
        AGameStateBase* GS = UGameplayStatics::GetGameState(this);
        IMDF_GameStateInterface* MDF_GS = Cast<IMDF_GameStateInterface>(GS);
        if (MDF_GS)
        {
            MDF_GS->SaveMDFData(ComponentGuid, HitHistory);
        }
    }

    NetMulticast_PlayEffects(HitQueue);
    OnRep_HitHistory(); 

    HitQueue.Empty();
}

void UMDF_DeformableComponent::OnRep_HitHistory()
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) return;

    UDynamicMeshComponent* MeshComp = Owner->FindComponentByClass<UDynamicMeshComponent>();
    if (!IsValid(MeshComp) || !IsValid(MeshComp->GetDynamicMesh())) return;

    int32 CurrentNum = HitHistory.Num();

    if (CurrentNum < LastAppliedIndex)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MDF] [Sync] 수리 명령(Reset) 감지!"));
        LastAppliedIndex = 0;
        InitializeDynamicMesh(); 
        return;
    }

    if (LastAppliedIndex == CurrentNum) return; 

    const double RadiusSq = FMath::Square((double)DeformRadius);
    const double InverseRadius = 1.0 / (double)DeformRadius;
    
    // [디버그용 변수] 가장 가까운 버텍스 거리 찾기
    double MinDebugDistSq = DBL_MAX;
    bool bAnyModified = false;

    // [디버그] 적용할 히트 포인트들을 월드 좌표로 찍어보기
    if (bShowDebugPoints)
    {
        for (int32 i = LastAppliedIndex; i < CurrentNum; ++i)
        {
            FVector WorldPos = MeshComp->GetComponentTransform().TransformPosition(HitHistory[i].LocalLocation);
            DrawDebugPoint(GetWorld(), WorldPos, 15.0f, FColor::Blue, false, 5.0f);
        }
    }

    MeshComp->GetDynamicMesh()->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh) 
    {
        for (int32 VertexID : EditMesh.VertexIndicesItr())
        {
            FVector3d VertexPos = EditMesh.GetVertex(VertexID);
            FVector3d TotalOffset(0.0, 0.0, 0.0);
            bool bModified = false;

            for (int32 i = LastAppliedIndex; i < CurrentNum; ++i)
            {
                const FMDFHitData& Hit = HitHistory[i];

                double DistSq = FVector3d::DistSquared(VertexPos, (FVector3d)Hit.LocalLocation);
                
                // [디버깅] 최소 거리 갱신
                if (DistSq < MinDebugDistSq) MinDebugDistSq = DistSq;

                if (DistSq < RadiusSq)
                {
                    double Distance = FMath::Sqrt(DistSq);
                    double Falloff = 1.0 - (Distance * InverseRadius);
                    
                    float DamageFactor = Hit.Damage * 0.05f; 
                    float CurrentStrength = DeformStrength * DamageFactor;

                    if (Hit.DamageTypeClass && MeleeDamageType && Hit.DamageTypeClass->IsChildOf(MeleeDamageType)) 
                        CurrentStrength *= 1.5f; 
                    else if (Hit.DamageTypeClass && RangedDamageType && Hit.DamageTypeClass->IsChildOf(RangedDamageType))
                        CurrentStrength *= 0.5f; 

                    TotalOffset += (FVector3d)Hit.LocalDirection * (double)(CurrentStrength * Falloff);
                    bModified = true;
                }
            }
            
            if (bModified)
            {
                EditMesh.SetVertex(VertexID, VertexPos + TotalOffset);
                bAnyModified = true;
            }
        }
    }, EDynamicMeshChangeType::GeneralEdit);

    // [결과 리포트] 왜 안 찌그러졌는지 알려줌
    if (!bAnyModified)
    {
        double MinDist = FMath::Sqrt(MinDebugDistSq);
        UE_LOG(LogTemp, Error, TEXT(">>> [변형 실패] 반경 내 버텍스 없음!"));
        UE_LOG(LogTemp, Error, TEXT("    - 설정된 반경: %.2f"), DeformRadius);
        UE_LOG(LogTemp, Error, TEXT("    - 가장 가까운 버텍스 거리: %.2f"), (float)MinDist);
        UE_LOG(LogTemp, Error, TEXT("    - 해결책: DeformRadius를 늘리거나 메쉬를 더 쪼개세요(Remesh)."));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT(">>> [변형 성공] 메쉬 변형 완료."));
    }

    LastAppliedIndex = CurrentNum;

    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(MeshComp->GetDynamicMesh(), FGeometryScriptCalculateNormalsOptions());
    MeshComp->UpdateCollision();
    MeshComp->NotifyMeshUpdated();
}

void UMDF_DeformableComponent::NetMulticast_PlayEffects_Implementation(const TArray<FMDFHitData>& NewHits)
{
    if (IsRunningDedicatedServer()) return;

    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) return;

    UDynamicMeshComponent* MeshComp = Owner->FindComponentByClass<UDynamicMeshComponent>();
    if (!IsValid(MeshComp)) return;
    
    const FTransform& ComponentTransform = MeshComp->GetComponentTransform();

    for (const FMDFHitData& Hit : NewHits)
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

// -----------------------------------------------------------------------------
// [Step 5 핵심 수정] 좌표 변환 로직 보정
// 액터(Actor) 기준이 아니라, 실제 변형되는 메쉬(DynamicMesh) 기준으로 좌표를 변환해야
// 오차 없이 정확히 그 자리가 찌그러집니다.
// -----------------------------------------------------------------------------
FVector UMDF_DeformableComponent::ConvertWorldToLocal(FVector WorldLocation)
{
    // 1. 다이나믹 메쉬 컴포넌트를 먼저 찾습니다.
    UDynamicMeshComponent* MeshComp = nullptr;
    if (GetOwner())
    {
        MeshComp = GetOwner()->FindComponentByClass<UDynamicMeshComponent>();
    }

    // 2. 메쉬가 있으면 '메쉬 기준' 로컬 좌표를 반환 (정확함)
    if (IsValid(MeshComp))
    {
        return MeshComp->GetComponentTransform().InverseTransformPosition(WorldLocation);
    }

    // 3. 없으면 액터 기준 (차선책)
    return IsValid(GetOwner()) ? GetOwner()->GetActorTransform().InverseTransformPosition(WorldLocation) : WorldLocation;
}

FVector UMDF_DeformableComponent::ConvertWorldDirectionToLocal(FVector WorldDirection)
{
    // 1. 다이나믹 메쉬 컴포넌트 찾기
    UDynamicMeshComponent* MeshComp = nullptr;
    if (GetOwner())
    {
        MeshComp = GetOwner()->FindComponentByClass<UDynamicMeshComponent>();
    }

    // 2. 메쉬 기준 로컬 방향 반환
    if (IsValid(MeshComp))
    {
        return MeshComp->GetComponentTransform().InverseTransformVector(WorldDirection);
    }

    // 3. 차선책
    return IsValid(GetOwner()) ? GetOwner()->GetActorTransform().InverseTransformVector(WorldDirection) : WorldDirection;
}

void UMDF_DeformableComponent::RepairMesh()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    HitHistory.Empty();
    LastAppliedIndex = 0;

    if (ComponentGuid.IsValid())
    {
        AGameStateBase* GS = UGameplayStatics::GetGameState(this);
        IMDF_GameStateInterface* MDF_GS = Cast<IMDF_GameStateInterface>(GS);
        if (MDF_GS)
        {
            MDF_GS->SaveMDFData(ComponentGuid, TArray<FMDFHitData>());
        }
    }

    InitializeDynamicMesh();
    UE_LOG(LogTemp, Warning, TEXT("[MDF] [Server] 수리 완료! (히스토리 초기화됨)"));
}