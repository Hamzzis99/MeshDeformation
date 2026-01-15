// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Weapon/MDF_LaserWeapon.cpp

#include "Weapons/MDF_LaserWeapon.h" // 헤더 경로 확인
#include "Components/MDF_MiniGameComponent.h" 
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

AMDF_LaserWeapon::AMDF_LaserWeapon()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false; 

    BatteryDrainRate = 20.0f;
    LaserColor = FColor::Red;
    FireRange = 3000.0f; 
}

void AMDF_LaserWeapon::BeginPlay()
{
    Super::BeginPlay();
}

void AMDF_LaserWeapon::StartFire()
{
    if (CurrentAmmo <= 0.0f) return;
    SetActorTickEnabled(true);
    UE_LOG(LogTemp, Log, TEXT("[레이저] 가동 시작"));
}

void AMDF_LaserWeapon::StopFire()
{
    SetActorTickEnabled(false);
    
    // [로직 변경] 마우스를 뗄 때 비로소 계산에 들어갑니다.
    if (CurrentTargetComp)
    {
       // 마지막으로 닿았던 곳(혹은 허공)을 넘겨주며 종료 신호
       // (컴포넌트가 알아서 마지막 유효 좌표를 쓸 것이므로 여기선 단순 신호용)
       FVector EndPos = MuzzleLocation ? MuzzleLocation->GetComponentLocation() + (MuzzleLocation->GetForwardVector() * FireRange) : GetActorLocation();
       
       CurrentTargetComp->EndMarking(EndPos);
       CurrentTargetComp = nullptr; // 소유권 해제
    }

    UE_LOG(LogTemp, Log, TEXT("[레이저] 가동 중지"));
}

void AMDF_LaserWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    float DrainAmount = BatteryDrainRate * DeltaTime;
    CurrentAmmo -= DrainAmount;

    if (CurrentAmmo <= 0.0f)
    {
       CurrentAmmo = 0.0f;
       StopFire();
       return;
    }

    ProcessLaserTrace();
}

void AMDF_LaserWeapon::ProcessLaserTrace()
{
    // 총구 기준 발사
    FVector Start = MuzzleLocation ? MuzzleLocation->GetComponentLocation() : GetActorLocation();
    FVector Forward = MuzzleLocation ? MuzzleLocation->GetForwardVector() : GetActorForwardVector();
    FVector End = Start + (Forward * FireRange);

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(GetOwner());

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);

    // 시각 효과
    if (bHit)
    {
        DrawDebugLine(GetWorld(), Start, HitResult.Location, LaserColor, false, 0.1f, 0, 2.0f);
        DrawDebugPoint(GetWorld(), HitResult.Location, 10.0f, LaserColor, false, 0.1f);
    }
    else
    {
        DrawDebugLine(GetWorld(), Start, End, LaserColor, false, 0.1f, 0, 1.0f);
    }

    // -------------------------------------------------------------------------
    // [핵심 로직] 타겟 유지 및 스위칭 (Stickiness Logic)
    // -------------------------------------------------------------------------
    
    UMDF_MiniGameComponent* NewWallComp = nullptr;

    // 벽을 맞췄을 때만 NewWallComp 갱신
    if (bHit && HitResult.GetActor())
    {
       NewWallComp = HitResult.GetActor()->FindComponentByClass<UMDF_MiniGameComponent>();
    }

    // 1. 벽을 맞췄다! (NewWallComp 존재)
    if (NewWallComp)
    {
        // 1-A. 다른 벽으로 넘어갔다? (Target Switching)
        if (CurrentTargetComp != NewWallComp)
        {
            // 기존 벽이 있었다면 즉시 계산하고 끝냄
            if (CurrentTargetComp)
            {
                CurrentTargetComp->EndMarking(HitResult.Location); 
            }

            // 새 벽 시작
            NewWallComp->StartMarking(HitResult.Location);
            CurrentTargetComp = NewWallComp;
        }
        // 1-B. 같은 벽이다?
        else
        {
            // 계속 그리기 (기록)
            CurrentTargetComp->UpdateMarking(HitResult.Location);
        }
    }
    // 2. 허공을 쏘고 있다! (NewWallComp 없음)
    else
    {
        // [중요] 타겟을 해제하지 않음! (CurrentTargetComp 유지)
        // 빗나가도 액터는 마지막 상태를 기억하고 있어야 하므로, 
        // 여기서는 아무 함수도 호출하지 않고 가만히 둡니다.
        // 나중에 손을 떼거나(StopFire), 다른 벽을 맞추면 그때 EndMarking이 불립니다.
    }
}