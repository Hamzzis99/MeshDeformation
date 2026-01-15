#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "InputActionValue.h"
#include "InventoryProjectCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class AMDF_BaseWeapon; // [New] 무기 클래스 전방 선언

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

    // [삭제됨] 하드코딩된 투사체 클래스는 이제 필요 없습니다.
    // TSubclassOf<class AInventoryProjectProjectile> ProjectileClass;

    // [New] 현재 장착 중인 무기 액터
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat", meta = (DisplayName = "현재 무기 (Current Weapon)"))
    TObjectPtr<AMDF_BaseWeapon> CurrentWeapon;

public:
    AInventoryProjectCharacter();  

    // [New] 외부(GameMode)에서 무기를 쥐어줄 때 사용하는 함수
    UFUNCTION(BlueprintCallable, Category="Combat", meta = (DisplayName = "무기 장착 (Equip Weapon)"))
    void EquipWeapon(TSubclassOf<AMDF_BaseWeapon> NewWeaponClass);

protected:
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    // [New] 사격 로직 분리 (누름/뗌)
    void OnFireStart(const FInputActionValue& Value);
    void OnFireStop(const FInputActionValue& Value);

    // [New] 서버 RPC (무기 사용 요청)
    UFUNCTION(Server, Reliable)
    void Server_StartFire();

    UFUNCTION(Server, Reliable)
    void Server_StopFire();

    // [삭제됨] 예전 발사 RPC
    // void Server_Fire(FVector Location, FRotator Rotation);

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