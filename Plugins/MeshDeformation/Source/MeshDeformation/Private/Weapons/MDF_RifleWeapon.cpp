// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Weapon/MDF_RifleWeapon.cpp

#include "Weapons/MDF_RifleWeapon.h"
#include "Components/MDF_MiniGameComponent.h" // [필수] 벽 컴포넌트 헤더
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

AMDF_RifleWeapon::AMDF_RifleWeapon()
{
    // 기본 라이플 스탯 설정
    FireRate = 0.1f;      // 연사 속도 빠름 (초당 10발)
    FireRange = 5000.0f;  // 사거리 50미터
    MaxAmmo = 300.0f;     // 탄약 넉넉하게
    CurrentAmmo = MaxAmmo;
    DamagePerShot = 10.0f; // 한 발당 10 데미지
}

void AMDF_RifleWeapon::Fire()
{
    // 1. 탄약 감소 및 소리 재생 (부모 클래스 기능 사용)
    Super::Fire();

    if (!MuzzleLocation) return;

    // 2. 레이캐스트 (총알 발사) 계산
    FVector Start = MuzzleLocation->GetComponentLocation();
    FVector Forward = MuzzleLocation->GetForwardVector();
    FVector End = Start + (Forward * FireRange);

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(GetOwner()); // 쏘는 사람 무시

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);

    // 3. 적중 처리
    if (bHit && HitResult.GetActor())
    {
        // [시각화] 총알 궤적 (노란색)
        DrawDebugLine(GetWorld(), Start, HitResult.Location, FColor::Yellow, false, 0.05f, 0, 1.0f);
        DrawDebugPoint(GetWorld(), HitResult.Location, 10.0f, FColor::Yellow, false, 0.05f);

        // ---------------------------------------------------------------------
        // [핵심] 미니게임 벽 타격 판정 -> 파괴 명령 전달
        // ---------------------------------------------------------------------
        UMDF_MiniGameComponent* WallComp = HitResult.GetActor()->FindComponentByClass<UMDF_MiniGameComponent>();
        
        if (WallComp)
        {
            // "벽아, 여기(HitInfo) 맞았고, 데미지는 이만큼이야!"
            WallComp->TryBreach(HitResult, DamagePerShot);
            
            // 로그 확인
            UE_LOG(LogTemp, Log, TEXT("[Rifle] 벽 타격! Actor: %s"), *HitResult.GetActor()->GetName());
        }
        else
        {
            // 일반 벽 타격
            UE_LOG(LogTemp, Verbose, TEXT("[Rifle] 일반 물체 타격"));
        }
    }
    else
    {
        // 허공 사격 (빨간선)
        DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, 0.05f, 0, 1.0f);
    }
}