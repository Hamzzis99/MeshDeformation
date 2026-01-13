// Gihyeon's Inventory Project
// TestGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Interface/MDF_GameStateInterface.h" 
#include "Components/MDF_DeformableComponent.h" 
#include "TestGameState.generated.h"

// [해결책] TMap 안에 TArray를 바로 넣을 수 없으므로, 구조체로 한 번 감싸줍니다.
USTRUCT(BlueprintType)
struct FMDFHitHistoryWrapper
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FMDFHitData> History;
};

UCLASS()
class INVENTORYPROJECT_API ATestGameState : public AGameStateBase, public IMDF_GameStateInterface
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// -------------------------------------------------------------------
	// [MDF_GameStateInterface 구현]
	// -------------------------------------------------------------------
	virtual void SaveMDFData(const FGuid& ID, const TArray<FMDFHitData>& History) override;
	virtual bool LoadMDFData(const FGuid& ID, TArray<FMDFHitData>& OutHistory) override;

	UFUNCTION(BlueprintCallable, Category="MDF|System")
	void Server_SaveAndMoveLevel(FName NextLevelName);

protected:
	void WriteDataToDisk();

	FString SaveSlotName = TEXT("MDF_SaveSlot");

	// [수정됨] TArray를 직접 쓰지 않고 Wrapper 구조체를 사용합니다.
	UPROPERTY()
	TMap<FGuid, FMDFHitHistoryWrapper> TestSavedMap;
};