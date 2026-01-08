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
    if (!HasAuthority()) return;
    
    if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;
    
    // [MeshDeformation] 접두사가 붙은 한글 로그
    UE_LOG(LogTemp, Warning, TEXT("[MeshDeformation] [서버] 타격 발생! 대상: %s / 좌표: %s"), *OtherActor->GetName(), *Hit.ImpactPoint.ToString());

    // ApplyPointDamage로 상세 충돌 데이터 전송
    UGameplayStatics::ApplyPointDamage(
        OtherActor,
        DamageValue,
        GetActorForwardVector(),
        Hit,
        GetInstigatorController(),
        this,
        nullptr
    );
    
    // [MeshDeformation] 접두사가 붙은 화면 디버깅 메시지
    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("[MeshDeformation] [타격] %s (데미지: %.0f)"), *OtherActor->GetName(), DamageValue);
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, DebugMsg);
    }
    
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Destroy();
}