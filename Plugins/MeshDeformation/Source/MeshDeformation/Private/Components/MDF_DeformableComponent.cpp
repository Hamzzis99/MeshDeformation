// Gihyeon's Deformation Project (Helluna)
// MDF_DeformableComponent.cpp

#include "Components/MDF_DeformableComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h" 
#include "Net/UnrealNetwork.h" 
#include "GameFramework/GameStateBase.h"

// 인터페이스 헤더 (프로젝트 경로에 맞게 확인 필요)
#include "Interface/MDF_GameStateInterface.h"

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
    
    // 변형 히스토리 복제
    DOREPLIFETIME(UMDF_DeformableComponent, HitHistory);
}

void UMDF_DeformableComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 1. 초기 메쉬 상태 설정
    InitializeDynamicMesh();
    
    // 리셋 안전 장치
    LastAppliedIndex = 0;
    
    AActor* Owner = GetOwner();

    // -------------------------------------------------------------------------
    // [Step 9: 인터페이스를 통한 데이터 복구 (Load)]
    // 월드 파티션 언로드/로드 시 GameState에서 기존 파괴 상태를 복구합니다.
    // -------------------------------------------------------------------------
    if (IsValid(Owner) && Owner->HasAuthority())
    {
        // 1. GUID가 없으면 이름 기반으로 생성 (안전장치)
        if (!ComponentGuid.IsValid())
        {
            FString UniqueName = Owner->GetName();
            FGuid::Parse(UniqueName, ComponentGuid);
            if (!ComponentGuid.IsValid()) ComponentGuid = FGuid::NewGuid();
        }

        // [DEBUG] 액터 생성 및 식별자 확인 (맵 이동 후 ID 유지 여부 확인용)
        UE_LOG(LogTemp, Log, TEXT("[MDF] [BeginPlay] Actor Initialized - Name: %s / GUID: %s"), 
            *Owner->GetName(), *ComponentGuid.ToString());

        // 2. 현재 월드의 GameState 가져오기
        AGameStateBase* GS = UGameplayStatics::GetGameState(this);

        // 3. 인터페이스(약속)를 지키는 GameState인지 확인 후 데이터 로드
        IMDF_GameStateInterface* MDF_GS = Cast<IMDF_GameStateInterface>(GS);
        
        if (MDF_GS)
        {
            TArray<FMDFHitData> SavedData;
            
            // [DEBUG] 로드 요청 시작 로그
            UE_LOG(LogTemp, Log, TEXT("[MDF] [Load Request] GameState에 데이터 요청 중... (GUID: %s)"), *ComponentGuid.ToString());

            // "제 ID(ComponentGuid)로 된 데이터가 있다면 주세요"
            if (MDF_GS->LoadMDFData(ComponentGuid, SavedData))
            {
                HitHistory = SavedData;
                
                // [DEBUG] 로드 성공 로그
                UE_LOG(LogTemp, Warning, TEXT("[MDF] [Load Success] 데이터 복원 성공! (Count: %d) - Actor: %s"), 
                    HitHistory.Num(), *Owner->GetName());
            }
            else
            {
                // [DEBUG] 데이터 없음 로그 (신규 생성 혹은 저장 안됨)
                UE_LOG(LogTemp, Log, TEXT("[MDF] [Load Result] 저장된 데이터 없음 (신규 액터이거나 기록 없음)."));
            }
        }
        else
        {
            // [DEBUG] 인터페이스 캐스팅 실패
            UE_LOG(LogTemp, Error, TEXT("[MDF] [Error] 현재 GameState가 IMDF_GameStateInterface를 상속받지 않았습니다!"));
        }
    }
    // -------------------------------------------------------------------------

    if (IsValid(Owner))
    {
       // 물리 및 리플리케이션 보정
       if (Owner->HasAuthority() && !Owner->GetIsReplicated())
       {
          Owner->SetReplicates(true);
          Owner->SetReplicateMovement(true); 
       }

       // 델리게이트 재바인딩 방지 및 등록
       Owner->OnTakePointDamage.RemoveDynamic(this, &UMDF_DeformableComponent::HandlePointDamage);
       Owner->OnTakePointDamage.AddDynamic(this, &UMDF_DeformableComponent::HandlePointDamage);
    }
    
    // Late Join 및 데이터 복구 후 강제 시각적 동기화
    if (HitHistory.Num() > 0)
    {
        OnRep_HitHistory(); 
    }
}

void UMDF_DeformableComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
    // 서버만 처리
    if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority()) return;
    if (!bIsDeformationEnabled || Damage <= 0.0f) return;

    // 공격자 필터링 (아군 오인 사격 방지 등)
    AActor* Attacker = DamageCauser;
    if (InstigatedBy && InstigatedBy->GetPawn())
    {
        Attacker = InstigatedBy->GetPawn();
    }

    if (IsValid(Attacker))
    {
        bool bIsEnemy = Attacker->ActorHasTag(TEXT("Enemy"));
        bool bIsTester = Attacker->ActorHasTag(TEXT("MDF_Test"));
        
        // 적이나 테스터가 아니면 무시
        if (!bIsEnemy && !bIsTester) return; 
    }

    // [Log: 데미지 수신 확인]
    FString DmgTypeName = IsValid(DamageType) ? DamageType->GetName() : TEXT("None");
    UE_LOG(LogTemp, Warning, TEXT("[MDF] [Damage In] 데미지 감지! Amount: %.1f / Type: %s / Attacker: %s"), 
        Damage, *DmgTypeName, *GetNameSafe(Attacker));

    // 다이나믹 메쉬 컴포넌트 찾기
    UDynamicMeshComponent* MeshComp = Cast<UDynamicMeshComponent>(FHitComponent);
    if (!IsValid(MeshComp))
    {
        MeshComp = GetOwner()->FindComponentByClass<UDynamicMeshComponent>();
    }

    if (IsValid(MeshComp))
    {
        // 큐에 데이터 임시 저장 (배치 처리를 위해)
        HitQueue.Add(FMDFHitData(
            ConvertWorldToLocal(HitLocation), 
            ConvertWorldDirectionToLocal(ShotFromDirection), 
            Damage, 
            DamageType ? DamageType->GetClass() : nullptr
        ));

        // 디버그 포인트 표시
        if (bShowDebugPoints)
        {
            DrawDebugPoint(GetWorld(), HitLocation, 10.0f, FColor::Red, false, 3.0f);
        }

        // 타이머가 돌고 있지 않다면 배치 처리 타이머 시작
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

    // 1. 실제 히스토리에 누적
    HitHistory.Append(HitQueue);

    // [DEBUG] 배치 처리 로그
    UE_LOG(LogTemp, Log, TEXT("[MDF] [Batch Process] %d개의 히트 데이터 처리 및 저장 시도."), HitQueue.Num());

    // -------------------------------------------------------------------------
    // [Step 9: 인터페이스를 통한 데이터 저장 (Save)]
    // 변형이 확정되었으므로 GameState에 백업합니다.
    // -------------------------------------------------------------------------
    if (ComponentGuid.IsValid())
    {
        AGameStateBase* GS = UGameplayStatics::GetGameState(this);
        IMDF_GameStateInterface* MDF_GS = Cast<IMDF_GameStateInterface>(GS);
        
        if (MDF_GS)
        {
            // "제 최신 상태(HitHistory)를 저장해주세요"
            MDF_GS->SaveMDFData(ComponentGuid, HitHistory);
            
            // [DEBUG] 저장 요청 로그
            UE_LOG(LogTemp, Log, TEXT("[MDF] [Save Request] GameState에 데이터 업데이트 요청 완료. (Total History: %d)"), HitHistory.Num());
        }
    }
    // -------------------------------------------------------------------------

    // 2. 이펙트 멀티캐스트 (시각 효과)
    NetMulticast_PlayEffects(HitQueue);

    // 3. 서버 측 메쉬 변형 적용 (충돌체 업데이트 등)
    OnRep_HitHistory(); 

    // 4. 큐 비우기
    HitQueue.Empty();
}

void UMDF_DeformableComponent::OnRep_HitHistory()
{
    AActor* Owner = GetOwner();
    if (!IsValid(Owner)) return;

    UDynamicMeshComponent* MeshComp = Owner->FindComponentByClass<UDynamicMeshComponent>();
    if (!IsValid(MeshComp) || !IsValid(MeshComp->GetDynamicMesh())) return;

    int32 CurrentNum = HitHistory.Num();

    // [Step 10] 수리 감지 로직: 현재 히스토리가 마지막 적용보다 적다면 리셋된 것임
    if (CurrentNum < LastAppliedIndex)
    {
        // [DEBUG] 수리 감지 로그
        UE_LOG(LogTemp, Warning, TEXT("[MDF] [Sync-Repair] 수리 명령(Reset) 감지! 메쉬를 원상복구합니다."));
        
        LastAppliedIndex = 0;
        InitializeDynamicMesh(); 
        // 여기서 return 하지 않고 0부터 다시 적용할 수도 있지만, 보통 수리는 완전 초기화를 의미함.
        return; 
    }

    // 변경사항이 없으면 리턴
    if (LastAppliedIndex == CurrentNum) return; 

    // [Log: 동기화 로그 - 역할 구분]
    FString NetRoleStr = UEnum::GetValueAsString(Owner->GetLocalRole());
    FString FuncRole = (Owner->HasAuthority()) ? TEXT("ListenServer/Host") : TEXT("RemoteClient");

    UE_LOG(LogTemp, Warning, TEXT("[MDF] [Sync Apply] 역할: %s (%s) / 적용 구간: %d ~ %d"), 
        *NetRoleStr, *FuncRole, LastAppliedIndex, CurrentNum - 1);

    const double RadiusSq = FMath::Square((double)DeformRadius);
    const double InverseRadius = 1.0 / (double)DeformRadius;
        
    // 메쉬 편집 (직접 버텍스 이동)
    MeshComp->GetDynamicMesh()->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh) 
    {
        for (int32 VertexID : EditMesh.VertexIndicesItr())
        {
            FVector3d VertexPos = EditMesh.GetVertex(VertexID);
            FVector3d TotalOffset(0.0, 0.0, 0.0);
            bool bModified = false;

            // 새로 추가된 히트 데이터만 순회하며 오프셋 계산
            for (int32 i = LastAppliedIndex; i < CurrentNum; ++i)
            {
                const FMDFHitData& Hit = HitHistory[i];

                double DistSq = FVector3d::DistSquared(VertexPos, (FVector3d)Hit.LocalLocation);
                
                if (DistSq < RadiusSq)
                {
                    double Distance = FMath::Sqrt(DistSq);
                    double Falloff = 1.0 - (Distance * InverseRadius);
                    
                    // 데미지에 비례한 강도 조절
                    float DamageFactor = Hit.Damage * 0.05f; 
                    float CurrentStrength = DeformStrength * DamageFactor;

                    // 데미지 타입에 따른 추가 보정
                    if (Hit.DamageTypeClass && MeleeDamageType && Hit.DamageTypeClass->IsChildOf(MeleeDamageType)) 
                    {
                        CurrentStrength *= 1.5f; // 근접 공격은 더 깊게
                    }
                    else if (Hit.DamageTypeClass && RangedDamageType && Hit.DamageTypeClass->IsChildOf(RangedDamageType))
                    {
                        CurrentStrength *= 0.5f; // 원거리는 얕게
                    }

                    // 방향으로 밀어넣기
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

    // 인덱스 갱신
    LastAppliedIndex = CurrentNum;

    // 노멀 재계산 및 충돌 업데이트 (매우 중요)
    UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(MeshComp->GetDynamicMesh(), FGeometryScriptCalculateNormalsOptions());
    MeshComp->UpdateCollision();
    MeshComp->NotifyMeshUpdated();
}

void UMDF_DeformableComponent::NetMulticast_PlayEffects_Implementation(const TArray<FMDFHitData>& NewHits)
{
    // 데디케이티드 서버는 이펙트 재생 안함
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

        // 파편 효과 (Niagara)
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

        // 스태틱 메시 원본을 다이나믹 메시로 복사 (초기화)
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

void UMDF_DeformableComponent::RepairMesh()
{
    // 1. 서버만 실행 가능
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    // 2. 데이터 초기화
    HitHistory.Empty();
    LastAppliedIndex = 0;

    // 3. GameState에도 초기화(삭제) 요청 [Step 10 추가]
    if (ComponentGuid.IsValid())
    {
        AGameStateBase* GS = UGameplayStatics::GetGameState(this);
        IMDF_GameStateInterface* MDF_GS = Cast<IMDF_GameStateInterface>(GS);
        if (MDF_GS)
        {
            // 빈 배열을 저장해서 영구 데이터도 초기화
            MDF_GS->SaveMDFData(ComponentGuid, TArray<FMDFHitData>());
            
            // [DEBUG] 초기화 요청 로그
            UE_LOG(LogTemp, Warning, TEXT("[MDF] [Repair] GameState 데이터 초기화 요청 완료 (GUID: %s)."), *ComponentGuid.ToString());
        }
    }

    // 4. 서버 쪽 모양 즉시 복구 (OnRep가 돌지 않을 수 있으므로 명시적 호출)
    InitializeDynamicMesh();

    UE_LOG(LogTemp, Warning, TEXT("[MDF] [Server] 수리 완료! (히스토리 초기화 및 저장소 클리어)"));
}