#include "InventoryProjectCharacter.h"
#include "InventoryProjectProjectile.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"

AInventoryProjectCharacter::AInventoryProjectCharacter()
{
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
}

void AInventoryProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
       EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
       EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
       EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AInventoryProjectCharacter::Move);
       EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AInventoryProjectCharacter::Look);
       EIC->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AInventoryProjectCharacter::Look);
       
       if (FireAction) {
          EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AInventoryProjectCharacter::OnFire);
       }
    }
}

void AInventoryProjectCharacter::OnFire(const FInputActionValue& Value)
{
    // [디버깅] 클라이언트 입력 확인 (하늘색)
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan, TEXT("Fire! (Client Request)"));
    UE_LOG(LogTemp, Log, TEXT("OnFire: Client requested shoot."));

    FVector Loc = GetActorLocation() + (GetActorForwardVector() * 100.0f);
    FRotator Rot = GetControlRotation();

    Server_Fire(Loc, Rot);
}

void AInventoryProjectCharacter::Server_Fire_Implementation(FVector Location, FRotator Rotation)
{
    // [디버깅] 서버 실행 확인 (노란색)
    UE_LOG(LogTemp, Warning, TEXT("Server: Attempting to spawn projectile for [%s]"), *GetName());

    if (ProjectileClass)
    {
       FActorSpawnParameters SpawnParams;
       SpawnParams.Owner = this;
       SpawnParams.Instigator = GetInstigator();

       GetWorld()->SpawnActor<AInventoryProjectProjectile>(ProjectileClass, Location, Rotation, SpawnParams);
    }
    else
    {
       // [디버깅] 클래스 미등록 에러 (빨간색)
       UE_LOG(LogTemp, Error, TEXT("Server Error: ProjectileClass is NULL! Check Character BP."));
       if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ERROR: Projectile Class NOT set in BP!"));
    }
}

// 이동/시선 처리
void AInventoryProjectCharacter::Move(const FInputActionValue& V) { FVector2D Vec = V.Get<FVector2D>(); DoMove(Vec.X, Vec.Y); }
void AInventoryProjectCharacter::Look(const FInputActionValue& V) { FVector2D Vec = V.Get<FVector2D>(); DoLook(Vec.X, Vec.Y); }
void AInventoryProjectCharacter::DoMove(float R, float F) { 
    if (GetController()) {
       const FRotator YawRot(0, GetControlRotation().Yaw, 0);
       AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), F);
       AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), R);
    }
}
void AInventoryProjectCharacter::DoLook(float Y, float P) { AddControllerYawInput(Y); AddControllerPitchInput(P); }
void AInventoryProjectCharacter::DoJumpStart() { Jump(); }
void AInventoryProjectCharacter::DoJumpEnd() { StopJumping(); }