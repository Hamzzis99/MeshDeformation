// Gihyeon's MeshDeformation Project
// File: Source/MeshDeformation/InventoryProjectCharacter.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "InputActionValue.h"
#include "InventoryProjectCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UMDF_WeaponComponent;

UCLASS(abstract)
class AInventoryProjectCharacter : public ACharacter
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
    class UMDF_WeaponComponent* WeaponComponent;
    
protected:
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* JumpAction;
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* MoveAction;
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* LookAction;
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* MouseLookAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* FireAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* EquipSlot1Action; 

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* EquipSlot2Action;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    TSubclassOf<class AInventoryProjectProjectile> ProjectileClass;

public:
    AInventoryProjectCharacter();  

protected:
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    /** 클라이언트 입력 함수 */
    void OnFireStart(const FInputActionValue& Value);
    void OnFireStop(const FInputActionValue& Value);
    void OnEquipSlot1();
    void OnEquipSlot2();

    // -------------------------------------------------------------------------
    // 서버 RPC (모든 실질적 로직은 서버에서 실행)
    // -------------------------------------------------------------------------
    
    /** [핵심] 사격 시작 요청 - 서버가 무기 유무를 판단함 */
    UFUNCTION(Server, Reliable)
    void Server_HandleFireStart();

    /** 사격 중지 요청 */
    UFUNCTION(Server, Reliable)
    void Server_HandleFireStop();

    /** 무기 교체 요청 */
    UFUNCTION(Server, Reliable)
    void Server_ToggleWeaponSlot(int32 SlotIndex);

public:
    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoMove(float Right, float Forward);
    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoLook(float Yaw, float Pitch);
    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoJumpStart();
    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoJumpEnd();

    FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
    FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};