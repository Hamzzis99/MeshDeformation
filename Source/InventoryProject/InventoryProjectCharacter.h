// Gihyeon's MeshDeformation Project

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "InputActionValue.h"
#include "Weapons/WeaponTestComponent/MDF_WeaponComponent.h"
#include "InventoryProjectCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;

UCLASS(abstract)
class AInventoryProjectCharacter : public ACharacter
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* FollowCamera;
    
protected:
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* JumpAction;
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* MoveAction;
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* LookAction;
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* MouseLookAction;

    /** 사격 입력 액션 (IA_Fire) */
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* FireAction;

    // [New] 무기 슬롯 교체 입력 (IA가 있다면 연결, 없다면 레거시 바인딩 사용)
    // 1번키, 2번키
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* EquipSlot1Action;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* EquipSlot2Action;

    // [Step 3 핵심] 무기 관리를 담당하는 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (DisplayName = "무기 관리 컴포넌트"))
    TObjectPtr<UMDF_WeaponComponent> WeaponComponent;

public:
    AInventoryProjectCharacter();  

protected:
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    // [사격] 컴포넌트로 전달
    void OnFireStart(const FInputActionValue& Value);
    void OnFireStop(const FInputActionValue& Value);

    // [교체] 1번, 2번 키 입력 처리
    void OnEquipSlot1();
    void OnEquipSlot2();

    // [서버 RPC]
    UFUNCTION(Server, Reliable)
    void Server_StartFire();

    UFUNCTION(Server, Reliable)
    void Server_StopFire();

    UFUNCTION(Server, Reliable)
    void Server_EquipSlot(int32 SlotIndex);

public:
    // 기존 조작 함수 유지
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