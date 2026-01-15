// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/InventoryProjectCharacter.cpp

#include "InventoryProjectCharacter.h"
#include "InventoryProjectProjectile.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "Weapons/WeaponTestComponent/MDF_WeaponComponent.h" 

AInventoryProjectCharacter::AInventoryProjectCharacter()
{
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

    WeaponComponent = CreateDefaultSubobject<UMDF_WeaponComponent>(TEXT("WeaponComponent"));
}

void AInventoryProjectCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent)) 
    {
        EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
        EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
        EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AInventoryProjectCharacter::Move);
        EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AInventoryProjectCharacter::Look);
        EIC->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AInventoryProjectCharacter::Look);
        
        if (FireAction) 
        {
            EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AInventoryProjectCharacter::OnFireStart);
            EIC->BindAction(FireAction, ETriggerEvent::Completed, this, &AInventoryProjectCharacter::OnFireStop);
        }

        if (EquipSlot1Action) EIC->BindAction(EquipSlot1Action, ETriggerEvent::Started, this, &AInventoryProjectCharacter::OnEquipSlot1);
        if (EquipSlot2Action) EIC->BindAction(EquipSlot2Action, ETriggerEvent::Started, this, &AInventoryProjectCharacter::OnEquipSlot2);
    }
}

// -----------------------------------------------------------------------------
// 입력 함수 (클라이언트에서 실행)
// -----------------------------------------------------------------------------

void AInventoryProjectCharacter::OnFireStart(const FInputActionValue& Value)
{
    // 클라이언트는 판단하지 않고 서버에 맡깁니다.
    Server_HandleFireStart();
}

void AInventoryProjectCharacter::OnFireStop(const FInputActionValue& Value)
{
    Server_HandleFireStop();
}

void AInventoryProjectCharacter::OnEquipSlot1() { Server_ToggleWeaponSlot(0); }
void AInventoryProjectCharacter::OnEquipSlot2() { Server_ToggleWeaponSlot(1); }

// -----------------------------------------------------------------------------
// 서버 RPC 구현 (실제 로직)
// -----------------------------------------------------------------------------

void AInventoryProjectCharacter::Server_HandleFireStart_Implementation()
{
    // 서버는 무기 장착 여부를 정확히 알고 있습니다.
    bool bHasWeapon = (WeaponComponent && WeaponComponent->GetCurrentWeapon());

    UE_LOG(LogTemp, Log, TEXT("[Server] HandleFireStart: HasWeapon = %s"), bHasWeapon ? TEXT("True") : TEXT("False"));

    if (bHasWeapon)
    {
        // 무기 시스템 사격 (레이저 등)
        WeaponComponent->StartFire();
    }
    else
    {
        // 맨손 상태 -> 기존 투사체 발사
        if (ProjectileClass)
        {
            FVector Loc = GetActorLocation() + (GetActorForwardVector() * 100.0f);
            FRotator Rot = GetControlRotation();
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            SpawnParams.Instigator = GetInstigator();
            GetWorld()->SpawnActor<AInventoryProjectProjectile>(ProjectileClass, Loc, Rot, SpawnParams);
            
            UE_LOG(LogTemp, Warning, TEXT("[Server] Spawning Projectile (No Weapon)"));
        }
    }
}

void AInventoryProjectCharacter::Server_HandleFireStop_Implementation()
{
    if (WeaponComponent)
    {
        WeaponComponent->StopFire();
    }
}

void AInventoryProjectCharacter::Server_ToggleWeaponSlot_Implementation(int32 SlotIndex)
{
    if (!WeaponComponent) return;
    
    int32 CurrentIndex = WeaponComponent->GetCurrentWeaponIndex();

    if (CurrentIndex == SlotIndex)
    {
        WeaponComponent->UnEquipWeapon();
        UE_LOG(LogTemp, Log, TEXT("[Server] Weapon UnEquipped"));
    }
    else
    {
        WeaponComponent->EquipWeaponByIndex(SlotIndex);
        UE_LOG(LogTemp, Log, TEXT("[Server] Weapon Equipped Slot: %d"), SlotIndex);
    }
}

// -----------------------------------------------------------------------------
// 이동/시선 처리 (기존 유지)
// -----------------------------------------------------------------------------
void AInventoryProjectCharacter::Move(const FInputActionValue& V) { FVector2D Vec = V.Get<FVector2D>(); DoMove(Vec.X, Vec.Y); }
void AInventoryProjectCharacter::Look(const FInputActionValue& V) { FVector2D Vec = V.Get<FVector2D>(); DoLook(Vec.X, Vec.Y); }

void AInventoryProjectCharacter::DoMove(float Right, float Forward) { 
    if (GetController()) {
       const FRotator YawRot(0, GetControlRotation().Yaw, 0);
       AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Forward);
       AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Right);
    }
}
void AInventoryProjectCharacter::DoLook(float Yaw, float Pitch) { 
    AddControllerYawInput(Yaw); 
    AddControllerPitchInput(Pitch); 
}
void AInventoryProjectCharacter::DoJumpStart() { Jump(); }
void AInventoryProjectCharacter::DoJumpEnd() { StopJumping(); }