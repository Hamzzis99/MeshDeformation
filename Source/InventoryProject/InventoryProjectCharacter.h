#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "InputActionValue.h"
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

    /** 발사할 투사체 클래스 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
    TSubclassOf<class AInventoryProjectProjectile> ProjectileClass;

public:
    AInventoryProjectCharacter();  

protected:
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    /** 사격 로직 */
    void OnFire(const FInputActionValue& Value);

    /** 서버 사격 RPC */
	UFUNCTION(Server, Reliable)
	void Server_Fire(FVector Location, FRotator Rotation);

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