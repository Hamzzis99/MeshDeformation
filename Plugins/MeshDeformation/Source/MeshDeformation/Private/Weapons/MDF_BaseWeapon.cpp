// Gihyeon's MeshDeformation Project

#include "Weapons/MDF_BaseWeapon.h"

#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AMDF_BaseWeapon::AMDF_BaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false; // 기본 무기는 틱이 필요 없음 (타이머 사용)

	// 1. 루트 컴포넌트 설정 (총구 위치 잡기용)
	MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	RootComponent = MuzzleLocation;

	// 2. 기본 스탯 설정 (기획 데이터 초기값)
	MaxAmmo = 100.0f;
	CurrentAmmo = 100.0f;
	FireRate = 0.2f;    // 0.2초에 한 발
	FireRange = 2000.0f; // 20미터

	// 멀티플레이를 위해 리플리케이션 켜기
	bReplicates = true;
}

void AMDF_BaseWeapon::BeginPlay()
{
	Super::BeginPlay();
    
	// 게임 시작 시 탄약 꽉 채우기
	CurrentAmmo = MaxAmmo;
}

void AMDF_BaseWeapon::StartFire()
{
	// 탄약이 없으면 발사 불가
	if (CurrentAmmo <= 0.0f)
	{
		// [한글 로그] 탄약 부족 알림
		UE_LOG(LogTemp, Warning, TEXT("[무기] 탄약이 부족합니다!"));
		return;
	}

	// 첫 발 즉시 발사
	Fire();

	// 두 번째 발부터는 타이머로 연사 (FireRate 간격)
	GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &AMDF_BaseWeapon::Fire, FireRate, true);
}

void AMDF_BaseWeapon::StopFire()
{
	// 타이머 정지 (사격 중지)
	GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
}

void AMDF_BaseWeapon::Fire()
{
	// [Base 로직] 실제 발사 구현은 없고, 탄약만 줄입니다.
	// 구체적인 총알/레이저 로직은 자식 클래스(Laser, Gun)에서 이 함수를 override해서 만듭니다.

	// 1. 탄약 감소
	ConsumeAmmo();

	// 2. 사운드 재생 (있다면)
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	// 3. 디버그 로그 (제대로 작동하는지 확인용)
	UE_LOG(LogTemp, Log, TEXT("[무기] 발사! 남은 탄약: %f"), CurrentAmmo);
}

void AMDF_BaseWeapon::ConsumeAmmo()
{
	CurrentAmmo -= 1.0f;

	if (CurrentAmmo <= 0.0f)
	{
		CurrentAmmo = 0.0f;
		StopFire(); // 탄약 다 떨어지면 강제 중지
		UE_LOG(LogTemp, Log, TEXT("[무기] 탄창이 비었습니다. 사격 중지."));
	}
}
