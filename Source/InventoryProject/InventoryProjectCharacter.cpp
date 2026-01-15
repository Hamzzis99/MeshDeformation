// Gihyeon's MeshDeformation Project

#include "InventoryProjectCharacter.h"
#include "InventoryProjectProjectile.h" // 유지
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"

// [New] 무기 베이스 헤더 포함
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

    // [Step 3] 무기 컴포넌트 생성
    WeaponComponent = CreateDefaultSubobject<UMDF_WeaponComponent>(TEXT("WeaponComponent"));
}

void AInventoryProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
       EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
       EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
       EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AInventoryProjectCharacter::Move);
       EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AInventoryProjectCharacter::Look);
       EIC->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AInventoryProjectCharacter::Look);
       
       // 사격 입력
       if (FireAction) {
          EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AInventoryProjectCharacter::OnFireStart);
          EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &AInventoryProjectCharacter::OnFireStop);
       }

       // [무기 교체 입력]
       // 만약 Enhanced Input에 Slot1, Slot2 액션을 아직 안 만드셨다면
       // 프로젝트 세팅 -> 입력 -> Action Mapping에서 "Slot1", "Slot2"를 추가하고
       // 아래처럼 레거시 바인딩을 쓰셔도 됩니다. (둘 다 안전하게 넣어둠)
       
       // 1. Enhanced Input 방식 (변수가 할당되어 있다면)
       if (EquipSlot1Action) EIC->BindAction(EquipSlot1Action, ETriggerEvent::Started, this, &AInventoryProjectCharacter::OnEquipSlot1);
       if (EquipSlot2Action) EIC->BindAction(EquipSlot2Action, ETriggerEvent::Started, this, &AInventoryProjectCharacter::OnEquipSlot2);
    }
    else
    {
        // 2. Legacy Input 방식 (백업)
        PlayerInputComponent->BindAction("Slot1", IE_Pressed, this, &AInventoryProjectCharacter::OnEquipSlot1);
        PlayerInputComponent->BindAction("Slot2", IE_Pressed, this, &AInventoryProjectCharacter::OnEquipSlot2);
    }
}

// -----------------------------------------------------------------------------
// [New] 무기 교체 로직 (1번, 2번 키)
// -----------------------------------------------------------------------------

void AInventoryProjectCharacter::OnEquipSlot1()
{
    // 로컬에서 누르면 서버로 요청
    Server_EquipSlot(0); // 0번 인덱스 (리더 무기 예상)
}

void AInventoryProjectCharacter::OnEquipSlot2()
{
    // 로컬에서 누르면 서버로 요청
    Server_EquipSlot(1); // 1번 인덱스 (슈터 무기 예상)
}

void AInventoryProjectCharacter::Server_EquipSlot_Implementation(int32 SlotIndex)
{
    if (WeaponComponent)
    {
        WeaponComponent->EquipWeaponByIndex(SlotIndex);
    }
}

// -----------------------------------------------------------------------------
// [New] 사격 입력 처리
// -----------------------------------------------------------------------------

void AInventoryProjectCharacter::OnFireStart(const FInputActionValue& Value)
{
    Server_StartFire();
}

void AInventoryProjectCharacter::OnFireStop(const FInputActionValue& Value)
{
    Server_StopFire();
}

void AInventoryProjectCharacter::Server_StartFire_Implementation()
{
    if (WeaponComponent)
    {
        WeaponComponent->StartFire();
    }
}

void AInventoryProjectCharacter::Server_StopFire_Implementation()
{
    if (WeaponComponent)
    {
        WeaponComponent->StopFire();
    }
}

// -----------------------------------------------------------------------------
// 기존 이동/시선 처리
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