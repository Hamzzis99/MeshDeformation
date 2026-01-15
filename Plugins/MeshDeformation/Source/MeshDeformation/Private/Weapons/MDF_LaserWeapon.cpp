// Gihyeon's MeshDeformation Project

#include "Weapons/MDF_LaserWeapon.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

AMDF_LaserWeapon::AMDF_LaserWeapon()
{
	// 레이저는 매 프레임 쏴야 하므로 Tick을 켭니다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // 쏠 때만 켭니다.

	// 기본값 설정
	BatteryDrainRate = 20.0f; // 초당 20 소모 (5초간 사용 가능)
	LaserColor = FColor::Red;
	FireRange = 3000.0f; // 레이저는 좀 더 길게
}

void AMDF_LaserWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void AMDF_LaserWeapon::StartFire()
{
	// 탄약(배터리) 체크
	if (CurrentAmmo <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[레이저] 배터리 방전!"));
		return;
	}

	// 1. Tick을 켜서 레이저 발사 시작
	SetActorTickEnabled(true);
	
	// 2. 발사 사운드 재생 등은 여기서 (나중에 추가)
	UE_LOG(LogTemp, Log, TEXT("[레이저] 가동 시작"));
}

void AMDF_LaserWeapon::StopFire()
{
	// 1. Tick을 꺼서 레이저 중지
	SetActorTickEnabled(false);
	
	UE_LOG(LogTemp, Log, TEXT("[레이저] 가동 중지"));
}

void AMDF_LaserWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 1. 배터리 소모
	// 프레임 시간(DeltaTime)에 비례해서 깎습니다.
	float DrainAmount = BatteryDrainRate * DeltaTime;
	CurrentAmmo -= DrainAmount;

	// 배터리 다 떨어지면 강제 종료
	if (CurrentAmmo <= 0.0f)
	{
		CurrentAmmo = 0.0f;
		StopFire();
		return;
	}

	// 2. 레이저 발사 로직 (LineTrace)
	ProcessLaserTrace();
}

void AMDF_LaserWeapon::ProcessLaserTrace()
{
	if (!MuzzleLocation) return;

	FVector Start = MuzzleLocation->GetComponentLocation();
	FVector End = Start + (MuzzleLocation->GetForwardVector() * FireRange);

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this); // 나 자신 무시
	Params.AddIgnoredActor(GetOwner()); // 총 주인(캐릭터) 무시

	// 레이캐스트 발사
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility, // 일단 시야 채널 사용
		Params
	);

	if (bHit)
	{
		// [적중 시] 빨간색 레이저 (시각 효과)
		DrawDebugLine(GetWorld(), Start, HitResult.Location, LaserColor, false, 0.1f, 0, 2.0f);
		DrawDebugPoint(GetWorld(), HitResult.Location, 10.0f, LaserColor, false, 0.1f);

		// [중요] 여기에 나중에 "벽에 약점 심기" 코드가 들어갑니다.
		// 지금은 로그만 찍습니다.
		// UE_LOG(LogTemp, Log, TEXT("[레이저] 지지는 중: %s"), *HitResult.GetActor()->GetName());
	}
	else
	{
		// [허공] 끝까지 나가는 레이저
		DrawDebugLine(GetWorld(), Start, End, LaserColor, false, 0.1f, 0, 1.0f);
	}
}