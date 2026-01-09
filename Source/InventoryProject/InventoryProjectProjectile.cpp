// Gihyeon's Inventory Project (Helluna)

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
    // 서버에서만 충돌 처리 (중요: 데미지 적용 및 변형 명령은 서버 권한 필요)
    if (!HasAuthority()) return;
    
    if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;
    
    // [MeshDeformation] 디버그 로그: 어떤 타입의 데미지가 들어가는지 확인
    FString DamageTypeName = ProjectileDamageType ? ProjectileDamageType->GetName() : TEXT("None");
    UE_LOG(LogTemp, Warning, TEXT("[MDF] [서버] 타격 발생! 대상: %s / 타입: %s"), *OtherActor->GetName(), *DamageTypeName);

    // ApplyPointDamage로 상세 충돌 데이터 및 '데미지 타입' 전송
    UGameplayStatics::ApplyPointDamage(
        OtherActor,
        DamageValue,
        GetActorForwardVector(),
        Hit,
        GetInstigatorController(),
        this,
        ProjectileDamageType // [핵심] 여기에 에디터에서 설정한 BP_DT_Ranged가 들어갑니다.
    );
    
    // [MeshDeformation] 화면 디버깅 메시지
    if (GEngine)
    {
        FString DebugMsg = FString::Printf(TEXT("[MDF] [타격] %s (데미지: %.0f, 타입: %s)"), *OtherActor->GetName(), DamageValue, *DamageTypeName);
        GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, DebugMsg);
    }
    
    CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Destroy();
}