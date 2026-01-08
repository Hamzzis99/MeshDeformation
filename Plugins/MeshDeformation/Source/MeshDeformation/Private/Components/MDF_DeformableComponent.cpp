#include "Components/MDF_DeformableComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h" // 디버그 드로잉을 위해 추가

UMDF_DeformableComponent::UMDF_DeformableComponent()
{
    PrimaryComponentTick.bCanEverTick = false; 

    // 컴포넌트 복제 활성화
    SetIsReplicatedByDefault(true);
}

void UMDF_DeformableComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (Owner)
    {
       // 복제 강제 활성화 (서버/클라이언트 동기화 보장)
       if (!Owner->GetIsReplicated())
       {
          Owner->SetReplicates(true);
          Owner->SetReplicateMovement(true); 
          UE_LOG(LogTemp, Warning, TEXT("[MeshDeformation] [설정 변경] %s 액터의 복제를 강제로 활성화했습니다."), *Owner->GetName());
       }

       // 데미지 델리게이트 바인딩
       Owner->OnTakePointDamage.AddDynamic(this, &UMDF_DeformableComponent::HandlePointDamage);
       UE_LOG(LogTemp, Log, TEXT("[MeshDeformation] [성공] %s 액터에 MDF 컴포넌트가 부착되었습니다."), *Owner->GetName());
    }
}

void UMDF_DeformableComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
    // 시스템 활성화 체크 및 데미지 유효성 검사
    if (!bIsDeformationEnabled || !DamagedActor || Damage <= 0.0f) return;

    // [Step 3-1] 월드 좌표를 로컬 좌표로 변환
    FVector LocalHitLocation = ConvertWorldToLocal(HitLocation);

    // [Step 3-2] 상세 분석 로그 출력
    UE_LOG(LogTemp, Warning, TEXT("[MeshDeformation] [데미지 수신] 대상: %s / 데미지: %.1f"), *DamagedActor->GetName(), Damage);
    UE_LOG(LogTemp, Log, TEXT("   > 월드 좌표: %s"), *HitLocation.ToString());
    UE_LOG(LogTemp, Log, TEXT("   > 로컬 좌표: %s"), *LocalHitLocation.ToString());

    // [Step 3-3] 시각적 디버깅
    if (bShowDebugPoints)
    {
        // 타격 지점에 3초 동안 유지되는 빨간 점 생성
        DrawDebugPoint(GetWorld(), HitLocation, 10.0f, FColor::Red, false, 3.0f);
        
        if (GEngine)
        {
           GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, 
              FString::Printf(TEXT("[MeshDeformation] 로컬 타격 좌표: %s"), *LocalHitLocation.ToString()));
        }
    }
}

FVector UMDF_DeformableComponent::ConvertWorldToLocal(FVector WorldLocation)
{
    if (!GetOwner()) return WorldLocation;

    // 역행렬 변환: $Local = World \times ActorInverseTransform$
    // 액터가 회전하거나 이동해도 메쉬 내부의 일관된 좌표를 얻을 수 있습니다.
    return GetOwner()->GetActorTransform().InverseTransformPosition(WorldLocation);
}