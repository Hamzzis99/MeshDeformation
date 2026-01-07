#include "InventoryProjectProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AInventoryProjectProjectile::AInventoryProjectProjectile()
{
	// 멀티플레이 복제 설정
	bReplicates = true;
	SetReplicateMovement(true);

	// 1. 충돌체 설정
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComponent->InitSphereRadius(5.0f);
	CollisionComponent->BodyInstance.SetCollisionProfileName("Projectile");
	CollisionComponent->OnComponentHit.AddDynamic(this, &AInventoryProjectProjectile::OnHit);
	RootComponent = CollisionComponent;

	// 2. 메쉬 설정
	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(RootComponent);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 3. 이동 설정
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 3000.f;
	ProjectileMovement->MaxSpeed = 3000.f;
	ProjectileMovement->bRotationFollowsVelocity = true;

	InitialLifeSpan = 3.0f; // 3초 뒤 자동 소멸
}

void AInventoryProjectProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 서버에서만 처리 (클라이언트는 무시)
	if (!HasAuthority())
	{
		return;
	}
	
	// 유효하지 않은 대상 무시
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner())
	{
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[발사체] %s 적중! 데미지 %.1f 적용"), *OtherActor->GetName(), DamageValue);
	
	// 즉시 충돌 비활성화 (중복 충돌 방지)
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	// 데미지 적용
	UGameplayStatics::ApplyDamage(
		OtherActor, 
		DamageValue, 
		GetInstigatorController(), 
		this, 
		UDamageType::StaticClass()
	);
	
	// 화면 디버깅
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, 
			FString::Printf(TEXT("HIT! %s ← %.0f"), *OtherActor->GetName(), DamageValue));
	}
	
	// 발사체 즉시 파괴
	Destroy();
}