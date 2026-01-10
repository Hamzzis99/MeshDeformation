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

    // [New] 현재 체력 복제
    DOREPLIFETIME(UMDF_DeformableComponent, CurrentHP);
}

void UMDF_DeformableComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 1. 초기 메쉬 상태 설정
    InitializeDynamicMesh();
    
    // 리셋 안전 장치
    LastAppliedIndex = 0;
    LoadRetryCount = 0; // 재시도 카운트 초기화
    
    // [New] 시작 시 체력을 최대치로 설정 (로드 데이터가 있으면 덮어씌워짐)
    CurrentHP = MaxHP;

    AActor* Owner = GetOwner();

    // -------------------------------------------------------------------------
    // [Step 9] 액터 이름 기반 데이터 복구 (Load)
    // -------------------------------------------------------------------------
    if (IsValid(Owner) && Owner->HasAuthority())
    {
        // 1. GUID가 없다면, '액터 이름'을 해싱하여 고정 ID 생성
        //    (예: "BP_TestActor_1" -> 항상 동일한 GUID 생성)
        if (!ComponentGuid.IsValid())
        {
            FString MyName = Owner->GetName();
            
            // 언리얼 내장 문자열 해시 함수 사용
            uint32 NameHash = GetTypeHash(MyName);
            
            // 해시값으로 GUID 채우기 (이름이 같으면 ID도 항상 같음)
            ComponentGuid = FGuid(NameHash, NameHash, NameHash, NameHash);
        }

        // [DEBUG] 식별자 확인 로그
        UE_LOG(LogTemp, Log, TEXT("[MDF] [BeginPlay] Actor: %s -> Auto-Generated GUID: %s"), 
            *Owner->GetName(), *ComponentGuid.ToString());

        // 2. GameState 접근 및 로드 (재시도 로직이 포함된 함수 호출)
        //    GameState 로딩 속도 차이(Race Condition)를 극복하기 위해 별도 함수로 분리함
        TryLoadDataFromGameState();
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

// [New Function] GameState가 준비될 때까지 기다렸다가 로드하는 함수
void UMDF_DeformableComponent::TryLoadDataFromGameState()
{
    AGameStateBase* GS = UGameplayStatics::GetGameState(this);
    IMDF_GameStateInterface* MDF_GS = Cast<IMDF_GameStateInterface>(GS);

    // 1. GameState 자체가 아직 없거나 인터페이스 준비가 안 됐다면 -> 짧게 대기 후 재시도
    if (!MDF_GS)
    {
        if (LoadRetryCount < 10) // 최대 2초 (0.2 * 10) 대기
        {
            LoadRetryCount++;
            GetWorld()->GetTimerManager().SetTimer(LoadRetryTimerHandle, this, &UMDF_DeformableComponent::TryLoadDataFromGameState, 0.2f, false);
        }
        return;
    }

    TArray<FMDFHitData> SavedData;
    float LoadedHP = MaxHP; // [New] 불러올 체력 임시 변수
    
    // 2. 데이터 로드 시도 (파라미터: ID, OutHistory, OutHP)
    // [DEBUG] 로드 요청 로그
    UE_LOG(LogTemp, Log, TEXT("[MDF] [Load Attempt] 데이터 요청 중... (GUID: %s, Retry: %d)"), *ComponentGuid.ToString(), LoadRetryCount);

    if (MDF_GS->LoadMDFData(ComponentGuid, SavedData, LoadedHP))
    {
        // 성공! 데이터 적용
        HitHistory = SavedData;
        CurrentHP = LoadedHP; // [New] 저장된 체력 적용
        
        // [DEBUG] 로드 성공 로그
        UE_LOG(LogTemp, Warning, TEXT("[MDF] [Load Success] %s 데이터 복원 완료! (Hit: %d, HP: %.1f)"), 
            *GetOwner()->GetName(), HitHistory.Num(), CurrentHP);
            
        // 즉시 메쉬 변형 적용
        OnRep_HitHistory();
    }
    else
    {
        // 3. 실패: GameState는 있지만 파일 로딩이 덜 끝났을 수 있음 -> 조금 길게 대기 후 재시도
        if (LoadRetryCount < 5) // 추가로 5번 더 시도 (약 2.5초)
        {
            LoadRetryCount++;
            // [DEBUG] 대기 로그
            UE_LOG(LogTemp, Log, TEXT("[MDF] [Load Pending] 데이터 없음. GameState 로딩 대기 중... (Retry: %d/5)"), LoadRetryCount);
            
            GetWorld()->GetTimerManager().SetTimer(LoadRetryTimerHandle, this, &UMDF_DeformableComponent::TryLoadDataFromGameState, 0.5f, false);
        }
        else
        {
            // [DEBUG] 최종 실패 로그 (정말 신규 액터임)
            UE_LOG(LogTemp, Log, TEXT("[MDF] [Load Final] 최종 결과: 저장된 데이터 없음 (신규 액터)."));
        }
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

    // [New] 체력 감소 로직
    CurrentHP = FMath::Clamp(CurrentHP - Damage, 0.0f, MaxHP);

    // [DEBUG] 요청하신 로그 출력
    UE_LOG(LogTemp, Warning, TEXT("[MDF] 타격! 데미지: %.1f / 남은 체력: %.1f"), Damage, CurrentHP);

    if (CurrentHP <= 0.0f)
    {
        // [To Do] 여기서 파괴 처리(Destroy) 등을 할 수 있음 (현재는 로그만 출력)
        UE_LOG(LogTemp, Error, TEXT("[MDF] 구조물이 파괴되었습니다!"));
    }

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
    UE_LOG(LogTemp, Log, TEXT("[MDF] [Batch Process] %d개의 히트 데이터 처리. 저장소 갱신 시도."), HitQueue.Num());

    // -------------------------------------------------------------------------
    // [Step 9: 인터페이스를 통한 데이터 저장 (Save)]
    // 변형이 확정되었으므로 GameState에 백업합니다.
    // * 덮어쓰기(Overwrite) 방식: 최신 히스토리를 통째로 저장하여 자동 갱신
    // -------------------------------------------------------------------------
    if (ComponentGuid.IsValid())
    {
        AGameStateBase* GS = UGameplayStatics::GetGameState(this);
        IMDF_GameStateInterface* MDF_GS = Cast<IMDF_GameStateInterface>(GS);
        
        if (MDF_GS)
        {
            // [New] 히스토리와 함께 '현재 체력(CurrentHP)'도 저장 요청
            MDF_GS->SaveMDFData(ComponentGuid, HitHistory, CurrentHP);
            
            // [DEBUG] 저장 요청 로그
            UE_LOG(LogTemp, Log, TEXT("[MDF] [Save Request] GameState 덮어쓰기 완료. (Hit: %d, HP: %.1f)"), HitHistory.Num(), CurrentHP);
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
        
        // 리턴하지 않고 진행 (만약 수리 후 남은 데미지가 있다면 처리하기 위해)
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

    // 2. 데이터 초기화 (로컬)
    HitHistory.Empty();
    LastAppliedIndex = 0;
    CurrentHP = MaxHP; // [New] 수리 시 체력도 100% 복구

    // 3. GameState에도 초기화(삭제) 요청 [Step 10 추가]
    //    빈 배열 + MaxHP를 '덮어쓰기' 하여 저장소의 데이터를 초기화합니다.
    if (ComponentGuid.IsValid())
    {
        AGameStateBase* GS = UGameplayStatics::GetGameState(this);
        IMDF_GameStateInterface* MDF_GS = Cast<IMDF_GameStateInterface>(GS);
        if (MDF_GS)
        {
            MDF_GS->SaveMDFData(ComponentGuid, TArray<FMDFHitData>(), MaxHP);
            
            // [DEBUG] 초기화 요청 로그
            UE_LOG(LogTemp, Warning, TEXT("[MDF] [Repair] GameState 데이터 및 HP 초기화 요청 완료 (GUID: %s)."), *ComponentGuid.ToString());
        }
    }

    // 4. 서버 쪽 모양 즉시 복구 (OnRep가 돌지 않을 수 있으므로 명시적 호출)
    InitializeDynamicMesh();

    UE_LOG(LogTemp, Warning, TEXT("[MDF] [Server] 수리 완료! (체력 복구됨)"));
}