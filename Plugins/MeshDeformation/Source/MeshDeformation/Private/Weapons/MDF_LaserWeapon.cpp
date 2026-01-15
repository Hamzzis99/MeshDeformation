// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/Weapon/MDF_LaserWeapon.cpp

#include "Weapons/MDF_LaserWeapon.h"
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

void AMDF_LaserWeapon::BeginPlay() { Super::BeginPlay(); }

void AMDF_LaserWeapon::StartFire()
{
    if (CurrentAmmo <= 0.0f) return;
    SetActorTickEnabled(true);
    UE_LOG(LogTemp, Log, TEXT("[레이저] 가동 시작"));
}

void AMDF_LaserWeapon::StopFire()
{
    SetActorTickEnabled(false);
    
    if (CurrentTargetComp)
    {
       FVector EndPos = MuzzleLocation ? MuzzleLocation->GetComponentLocation() + (MuzzleLocation->GetForwardVector() * FireRange) : GetActorLocation();
       CurrentTargetComp->EndMarking(EndPos);
       CurrentTargetComp = nullptr;
    }
    UE_LOG(LogTemp, Log, TEXT("[레이저] 가동 중지"));
}

void AMDF_LaserWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    CurrentAmmo -= BatteryDrainRate * DeltaTime;
    if (CurrentAmmo <= 0.0f) { CurrentAmmo = 0.0f; StopFire(); return; }
    
    ProcessLaserTrace();
}

void AMDF_LaserWeapon::ProcessLaserTrace()
{
    FVector Start = MuzzleLocation ? MuzzleLocation->GetComponentLocation() : GetActorLocation();
    FVector Forward = MuzzleLocation ? MuzzleLocation->GetForwardVector() : GetActorForwardVector();
    FVector End = Start + (Forward * FireRange);

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(GetOwner());

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);

    // [디버깅] 레이저 궤적 그리기
    if (bHit)
    {
        DrawDebugLine(GetWorld(), Start, HitResult.Location, LaserColor, false, -1.0f, 0, 2.0f);
        // 맞은 지점에 진한 포인트 표시
        DrawDebugPoint(GetWorld(), HitResult.Location, 10.0f, FColor::Blue, false, -1.0f);
        
        // [중요] 좌표 로그 출력 (매 프레임)
        UE_LOG(LogTemp, Warning, TEXT("[LaserHit] World: %s / Actor: %s"), *HitResult.Location.ToString(), *HitResult.GetActor()->GetName());
    }
    else
    {
        DrawDebugLine(GetWorld(), Start, End, LaserColor, false, -1.0f, 0, 1.0f);
    }

    // -------------------------------------------------------------------------
    // 타겟 연동 로직
    // -------------------------------------------------------------------------
    UMDF_MiniGameComponent* NewWallComp = nullptr;
    if (bHit && HitResult.GetActor())
    {
        NewWallComp = HitResult.GetActor()->FindComponentByClass<UMDF_MiniGameComponent>();
    }

    if (NewWallComp)
    {
        if (CurrentTargetComp != NewWallComp)
        {
            if (CurrentTargetComp) CurrentTargetComp->EndMarking(HitResult.Location); 
            NewWallComp->StartMarking(HitResult.Location);
            CurrentTargetComp = NewWallComp;
        }
        else
        {
            CurrentTargetComp->UpdateMarking(HitResult.Location);
        }
    }
}