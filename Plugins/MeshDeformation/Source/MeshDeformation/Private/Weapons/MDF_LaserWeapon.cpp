// Gihyeon's MeshDeformation Project

#include "Weapons/MDF_LaserWeapon.h" 
#include "Components/MDF_MiniGameComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AMDF_LaserWeapon::AMDF_LaserWeapon()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false; // 쏠 때만 켭니다.

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
    if (CurrentAmmo <= 0.0f)
    {
       UE_LOG(LogTemp, Warning, TEXT("[레이저] 배터리 방전!"));
       return;
    }

    SetActorTickEnabled(true);
    UE_LOG(LogTemp, Log, TEXT("[레이저] 가동 시작"));
}

void AMDF_LaserWeapon::StopFire()
{
    SetActorTickEnabled(false);
    
    // [중요] 쏘다가 멈췄으면, 마킹도 끝내야 함
    if (CurrentTargetComp)
    {
       // 마지막 지점을 계산해야 합니다.
       // 레이저가 꺼지는 순간의 시점 정보를 가져옵니다.
       FVector TraceStart, TraceDir;
       APlayerController* PC = Cast<APlayerController>(GetOwner()->GetInstigatorController());
       
       FVector EndPos;
       if (PC)
       {
           int32 SizeX, SizeY;
           PC->GetViewportSize(SizeX, SizeY);
           UGameplayStatics::DeprojectScreenToWorld(PC, FVector2D(SizeX/2.f, SizeY/2.f), TraceStart, TraceDir);
           EndPos = TraceStart + (TraceDir * FireRange);
       }
       else
       {
           EndPos = GetActorLocation() + (GetActorForwardVector() * FireRange);
       }

       // [수정됨] FinishMarking -> EndMarking으로 변경
       CurrentTargetComp->EndMarking(EndPos);
       CurrentTargetComp = nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("[레이저] 가동 중지"));
}

void AMDF_LaserWeapon::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 1. 배터리 소모
    float DrainAmount = BatteryDrainRate * DeltaTime;
    CurrentAmmo -= DrainAmount;

    if (CurrentAmmo <= 0.0f)
    {
       CurrentAmmo = 0.0f;
       StopFire();
       return;
    }

    // 2. 레이저 로직 수행
    ProcessLaserTrace();
}

void AMDF_LaserWeapon::ProcessLaserTrace()
{
    // 1. 플레이어 컨트롤러 가져오기 (화면 좌표 계산용)
    APlayerController* PC = Cast<APlayerController>(GetOwner()->GetInstigatorController());
    if (!PC) return;

    // 2. 뷰포트 중앙 좌표 구하기
    int32 ViewportSizeX, ViewportSizeY;
    PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
    FVector2D ViewportCenter(ViewportSizeX / 2.f, ViewportSizeY / 2.f);

    // 3. 화면 중앙을 월드 좌표로 변환 (Deproject)
    FVector TraceStart;
    FVector TraceDir; // 카메라가 보는 방향
    if (!UGameplayStatics::DeprojectScreenToWorld(PC, ViewportCenter, TraceStart, TraceDir)) return;

    // 4. 레이저 끝 지점 계산
    FVector TraceEnd = TraceStart + (TraceDir * FireRange);

    // 5. 실제 레이저 쏘기 (LineTrace)
    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(GetOwner()); // 나 자신(캐릭터) 무시

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params);

    // [시각 효과 보정]
    // 총구(Muzzle)에서 나가는 것처럼 보이게 하려면?
    FVector VisualStart = MuzzleLocation ? MuzzleLocation->GetComponentLocation() : GetActorLocation();
    FVector VisualEnd = bHit ? HitResult.Location : TraceEnd;

    if (bHit)
    {
        // 디버그 라인: 총구 -> 맞은 곳
        DrawDebugLine(GetWorld(), VisualStart, HitResult.Location, LaserColor, false, 0.1f, 0, 2.0f);
        DrawDebugPoint(GetWorld(), HitResult.Location, 10.0f, LaserColor, false, 0.1f);
    }
    else
    {
        // 디버그 라인: 총구 -> 허공
        DrawDebugLine(GetWorld(), VisualStart, TraceEnd, LaserColor, false, 0.1f, 0, 1.0f);
    }

    // -------------------------------------------------------------------------
    // [연동 로직] 미니게임 컴포넌트 찾기 및 업데이트
    // -------------------------------------------------------------------------
    UMDF_MiniGameComponent* HitWallComp = nullptr;
    if (bHit && HitResult.GetActor())
    {
        HitWallComp = HitResult.GetActor()->FindComponentByClass<UMDF_MiniGameComponent>();
    }

    // [상태 머신 로직 수정] Start / Update / End를 정확히 호출

    // 1. 타겟이 바뀌었거나(다른 벽), 벽이 아닌 곳(허공)을 보고 있을 때
    if (HitWallComp != CurrentTargetComp)
    {
        // 기존에 칠하던 벽이 있었다면 종료 처리 (EndMarking)
        if (CurrentTargetComp)
        {
            // 허공을 보고 있다면 TraceEnd, 다른 벽을 보고 있다면 HitResult.Location을 넘김
            FVector EndPos = bHit ? HitResult.Location : TraceEnd;
            CurrentTargetComp->EndMarking(EndPos); 
        }

        // 새로운 벽을 보기 시작했다면 시작 처리 (StartMarking)
        if (HitWallComp)
        {
            HitWallComp->StartMarking(HitResult.Location);
        }

        CurrentTargetComp = HitWallComp; // 타겟 갱신
    }
    // 2. 같은 벽을 계속 보고 있을 때 -> 업데이트 (UpdateMarking)
    else if (CurrentTargetComp)
    {
        // [수정됨] UpdateLaserHit -> UpdateMarking
        CurrentTargetComp->UpdateMarking(HitResult.Location);
    }
}