#include "Components/DeformableComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"

UDeformableComponent::UDeformableComponent()
{
	PrimaryComponentTick.bCanEverTick = true; 
	SetIsReplicatedByDefault(true);
}

void UDeformableComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner)
	{
		// 복제 강제 활성화
		if (!Owner->GetIsReplicated())
		{
			Owner->SetReplicates(true);
			Owner->SetReplicateMovement(true); 
			UE_LOG(LogTemp, Warning, TEXT("[MeshDeformation] [설정 변경] %s 액터의 복제를 강제로 활성화했습니다."), *Owner->GetName());
		}

		Owner->OnTakePointDamage.AddDynamic(this, &UDeformableComponent::HandlePointDamage);
		UE_LOG(LogTemp, Log, TEXT("[MeshDeformation] [성공] %s 액터에 컴포넌트가 부착되었습니다."), *Owner->GetName());
	}
}

void UDeformableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UDeformableComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	if (!bIsDeformationEnabled || Damage <= 0.0f) return;

	UE_LOG(LogTemp, Warning, TEXT("[MeshDeformation] [데미지 수신] 대상: %s / 위치: %s / 데미지: %.1f"), 
		*DamagedActor->GetName(), *HitLocation.ToString(), Damage);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, 
			FString::Printf(TEXT("[MeshDeformation] 데미지 감지 성공! (%.1f)"), Damage));
	}
}