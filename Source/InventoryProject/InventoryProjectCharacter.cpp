#include "InventoryProjectCharacter.h"
#include "InventoryProjectProjectile.h" // 이건 필요 없을 수도 있으나 일단 유지
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"

// [New] 무기 헤더 포함 필수
#include "Weapons/MDF_BaseWeapon.h"

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
       
       // [수정] 사격 입력: 누름(Started)과 뗌(Completed)을 분리
       if (FireAction) {
          EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AInventoryProjectCharacter::OnFireStart);
          EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &AInventoryProjectCharacter::OnFireStop);
       }
    }
}

// -----------------------------------------------------------------------------
// [New] 무기 장착 시스템
// -----------------------------------------------------------------------------
void AInventoryProjectCharacter::EquipWeapon(TSubclassOf<AMDF_BaseWeapon> NewWeaponClass)
{
    // 서버에서만 실행 (멀티플레이 안전성)
    if (!HasAuthority()) return;

    // 기존 무기 제거
    if (CurrentWeapon)
    {
        CurrentWeapon->Destroy();
        CurrentWeapon = nullptr;
    }

    if (!NewWeaponClass) return;

    // 새 무기 스폰
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = this;
    
    AMDF_BaseWeapon* NewWeapon = GetWorld()->SpawnActor<AMDF_BaseWeapon>(NewWeaponClass, GetActorLocation(), GetActorRotation(), Params);

    if (NewWeapon)
    {
        // 소켓 이름은 스켈레탈 메시에서 만든 이름과 일치해야 함 ("WeaponSocket")
        NewWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, TEXT("WeaponSocket"));
        CurrentWeapon = NewWeapon;

        UE_LOG(LogTemp, Log, TEXT("[Character] Equipped Weapon: %s"), *NewWeapon->GetName());
    }
}

// -----------------------------------------------------------------------------
// [New] 사격 입력 처리 (Client -> Server -> Weapon)
// -----------------------------------------------------------------------------

void AInventoryProjectCharacter::OnFireStart(const FInputActionValue& Value)
{
    // 로컬 즉시 반응 (선택 사항: 애니메이션 등)
    // 서버로 요청
    Server_StartFire();
}

void AInventoryProjectCharacter::OnFireStop(const FInputActionValue& Value)
{
    // 서버로 요청
    Server_StopFire();
}

void AInventoryProjectCharacter::Server_StartFire_Implementation()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->StartFire(); // 무기에게 "쏴!" 명령
    }
}

void AInventoryProjectCharacter::Server_StopFire_Implementation()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->StopFire(); // 무기에게 "멈춰!" 명령
    }
}

// -----------------------------------------------------------------------------
// 기존 이동/시선 처리 (유지)
// -----------------------------------------------------------------------------
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