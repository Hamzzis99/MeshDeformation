// Gihyeon's MeshDeformation Project

#include "Weapons/WeaponTestComponent/MDF_WeaponComponent.h"
#include "Weapons/MDF_BaseWeapon.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

UMDF_WeaponComponent::UMDF_WeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	WeaponAttachSocketName = TEXT("WeaponSocket"); // 기본값
}

void UMDF_WeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// [선택 사항] 게임 시작 시 자동으로 1번 무기(0번 인덱스) 장착
	// EquipWeaponByIndex(0);
}

void UMDF_WeaponComponent::EquipWeaponByIndex(int32 SlotIndex)
{
	// 1. 인덱스 유효성 검사
	if (!WeaponSlots.IsValidIndex(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponComp] 잘못된 슬롯 번호입니다: %d"), SlotIndex);
		return;
	}

	TSubclassOf<AMDF_BaseWeapon> TargetWeaponClass = WeaponSlots[SlotIndex];
	if (!TargetWeaponClass) return;

	// 2. 주인(Owner) 확인 - 무기는 캐릭터 손에 붙어야 하니까요
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	// 3. 기존 무기 제거 (교체 로직)
	if (CurrentWeaponActor)
	{
		// 쏘고 있었다면 멈추고
		CurrentWeaponActor->StopFire();
		// 파괴
		CurrentWeaponActor->Destroy();
		CurrentWeaponActor = nullptr;
	}

	// 4. 새 무기 스폰 (서버 권한 체크는 캐릭터에서 하거나 여기서 추가 가능)
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;

	AMDF_BaseWeapon* NewWeapon = GetWorld()->SpawnActor<AMDF_BaseWeapon>(TargetWeaponClass, OwnerCharacter->GetActorLocation(), OwnerCharacter->GetActorRotation(), SpawnParams);

	if (NewWeapon)
	{
		NewWeapon->AttachToComponent(OwnerCharacter->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, WeaponAttachSocketName);
        
		CurrentWeaponActor = NewWeapon;
		
		FString WeaponName = NewWeapon->GetName();
        
		// 화면 상단 출력 (Cyan 색상)
		if (GEngine)
		{
			FString Msg = FString::Printf(TEXT("무기 교체 성공: %s (슬롯: %d)"), *WeaponName, SlotIndex);
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Msg);
		}

		// 로그 출력
		UE_LOG(LogTemp, Log, TEXT("[WeaponSystem] %s(으)로 무기가 변경되었습니다."), *WeaponName);
	}
}

void UMDF_WeaponComponent::StartFire()
{
	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->StartFire();
	}
}

void UMDF_WeaponComponent::StopFire()
{
	if (CurrentWeaponActor)
	{
		CurrentWeaponActor->StopFire();
	}
}