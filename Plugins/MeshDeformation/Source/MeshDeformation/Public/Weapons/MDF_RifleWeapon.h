// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Weapon/MDF_RifleWeapon.h

#pragma once

#include "CoreMinimal.h"
#include "Weapons/MDF_BaseWeapon.h"
#include "MDF_RifleWeapon.generated.h"

/**
 * [Step 5 Test] 슈터용 라이플
 * - 레이저 대신 즉발(HitScan) 사격을 합니다.
 * - MDF_MiniGameComponent를 맞추면 TryBreach를 호출해 벽을 부숩니다.
 */
UCLASS()
class MESHDEFORMATION_API AMDF_RifleWeapon : public AMDF_BaseWeapon
{
	GENERATED_BODY()
    
public:
	AMDF_RifleWeapon();

protected:
	// BaseWeapon의 Fire 함수를 덮어씁니다. (실제 사격 로직)
	virtual void Fire() override;

protected:
	// [설정] 한 발당 데미지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MDF|Stats", meta = (DisplayName = "발당 데미지"))
	float DamagePerShot;

	// [설정] 사격 이펙트 (파티클 등은 나중에, 일단 디버그 선으로 대체)
};