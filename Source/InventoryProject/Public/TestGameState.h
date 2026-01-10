// Gihyeon's Inventory Project

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"

// [MDF] 인터페이스 헤더 포함 (경로가 안 맞으면 수정 필요)
#include "Interface/MDF_GameStateInterface.h"
#include "Interface/MDF_GameStateInterface.h" 

#include "TestGameState.generated.h"

/**
 * 테스트용 임시 GameState
 */
UCLASS()
class INVENTORYPROJECT_API ATestGameState : public AGameStateBase, public IMDF_GameStateInterface
{
	GENERATED_BODY()
    
public:
	// =========================================================================
	// [MDF Interface] 테스트 구현
	// =========================================================================
	virtual void SaveMDFData(const FGuid& ID, const TArray<FMDFHitData>& Data) override;
	virtual bool LoadMDFData(const FGuid& ID, TArray<FMDFHitData>& OutData) override;

protected:
	// 데이터를 저장할 테스트용 메모리 장부
	TMap<FGuid, TArray<FMDFHitData>> TestSavedMap;
};