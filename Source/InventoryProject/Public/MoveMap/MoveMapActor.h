// Gihyeon's Inventory Project

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MoveMapActor.generated.h"

UCLASS()
class INVENTORYPROJECT_API AMoveMapActor : public AActor
{
	GENERATED_BODY()

public:
	AMoveMapActor();

protected:
	virtual void BeginPlay() override;

public:
	// 상호작용 함수 (캐릭터가 호출)
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Interact();

	// 서버 RPC
	UFUNCTION(Server, Reliable)
	void Server_RequestMove();

public:
	// 이동할 맵 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Settings", meta = (ExposeOnSpawn = "true"))
	FName NextLevelName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComp;
};