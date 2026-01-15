// Gihyeon's MeshDeformation Project

#pragma once

#include "CoreMinimal.h"
#include "Weapons/MDF_BaseWeapon.h"
#include "MDF_LaserWeapon.generated.h"

/**
 * [Step 3] 리더용 레이저 무기 (Marker)
 * - 특징: 마우스를 누르고 있는 동안 계속 나감 (Tick 사용)
 * - 기능: 벽에 닿으면 빨간색 선을 그리고, 나중에 '약점'을 심게 됨.
 */
UCLASS()
class MESHDEFORMATION_API AMDF_LaserWeapon : public AMDF_BaseWeapon
{
	GENERATED_BODY()
	
public:
	AMDF_LaserWeapon();

protected:
	virtual void BeginPlay() override;

public:
	// 매 프레임 레이저를 쏘기 위해 Tick을 사용합니다.
	virtual void Tick(float DeltaTime) override;

	// 부모의 Start/Stop을 오버라이드해서 타이머 대신 Tick을 켜고 끕니다.
	virtual void StartFire() override;
	virtual void StopFire() override;

protected:
	// 실제 레이저 로직 (Tick에서 호출됨)
	void ProcessLaserTrace();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MDF|Stats", meta = (DisplayName = "초당 배터리 소모량"))
	float BatteryDrainRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MDF|Effects", meta = (DisplayName = "레이저 색상 (Debug)"))
	FColor LaserColor;
};