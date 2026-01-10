// Gihyeon's Inventory Project

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Interface/MDF_GameStateInterface.h"
#include "Save/MDF_SaveActor.h" 
#include "TestGameState.generated.h"

UCLASS()
class INVENTORYPROJECT_API ATestGameState : public AGameStateBase, public IMDF_GameStateInterface
{
	GENERATED_BODY()
    
public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// MDF Interface
	virtual void SaveMDFData(const FGuid& ID, const TArray<FMDFHitData>& Data) override;
	virtual bool LoadMDFData(const FGuid& ID, TArray<FMDFHitData>& OutData) override;

	// [Step 12: 추가] 저장 후 맵 이동 함수
	UFUNCTION(BlueprintCallable, Category = "MDF Logic")
	void Server_SaveAndMoveLevel(FName NextLevelName);

protected:
	TMap<FGuid, TArray<FMDFHitData>> TestSavedMap;
	const FString SaveSlotName = TEXT("MDF_TestSaveSlot");
};