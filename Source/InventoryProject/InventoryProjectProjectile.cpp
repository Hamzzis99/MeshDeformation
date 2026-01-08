#include "InventoryProjectProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AInventoryProjectProjectile::AInventoryProjectProjectile()
{
    bReplicates = true;
    SetReplicateMovement(true);

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComponent->InitSphereRadius(5.0f);
    CollisionComponent->BodyInstance.SetCollisionProfileName("Projectile");
    CollisionComponent->OnComponentHit.AddDynamic(this, &AInventoryProjectProjectile::OnHit);
    RootComponent = CollisionComponent;

    ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
    ProjectileMesh->SetupAttachment(RootComponent);
    ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
    ProjectileMovement->UpdatedComponent = CollisionComponent;
    ProjectileMovement->InitialSpeed = 3000.f;
    ProjectileMovement->MaxSpeed = 3000.f;
    ProjectileMovement->bRotationFollowsVelocity = true;

    InitialLifeSpan = 3.0f;
}

void AInventoryProjectProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // [서버 체크] 데미지 처리는 서버에서만 수행합니다.
    if (!HasAuthority()) return;
    
    // [유효성 체크] 대상이 있고, 자기 자신이 아닐 때만 처리합니다.
    if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;
    
    // [한글 로그] 상세한 타격 정보를 출력합니다.
    UE_LOG(LogTemp, Warning, TEXT("[서버] 타격 발생! 대상: %s / 좌표: %s"), *OtherActor->GetName(), *Hit.ImpactPoint.ToString());

    // [Step 0 핵심] ApplyPointDamage 호출
    // 찌그러짐을 위해 'Hit' 데이터(좌표, 법선 등)를 통째로 넘겨줍니다.
    UGameplayStatics::ApplyPointDamage(
        OtherActor,                // 데미지를 받을 대상
        DamageValue,               // 데미지 수치
        GetActorForwardVector(),   // 타격 방향 (발사체 진행 방향)
        Hit,                       // 충돌 상세 정보 (좌표, 노멀 등 포함)
        GetInstigatorController(),  // 공격 유발자
        this,                      // 데미지 원인 액터
        nullptr                    // 데미지 타입 (기본)
    );
    
    // [화면 디버깅]
    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("[타격] %s (데미지: %.0f)"), *OtherActor->GetName(), DamageValue);
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, DebugMsg);
    }
    
    // 중복 충돌 방지를 위해 콜리전 끄고 파괴
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Destroy();
}