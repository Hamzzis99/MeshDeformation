// Gihyeon's Inventory Project
// TestGameState.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Interface/MDF_GameStateInterface.h" // 인터페이스 헤더 필수
#include "TestGameState.generated.h"

/**
 * 게임 상태를 관리하는 클래스
 * MDF 인터페이스를 상속받아 변형 데이터를 저장/로드합니다.
 */
UCLASS()
class INVENTORYPROJECT_API ATestGameState : public AGameStateBase, public IMDF_GameStateInterface
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// -------------------------------------------------------------------------
	// [MDF Interface 구현] 함수 모양을 인터페이스와 똑같이 맞춰야 합니다!
	// -------------------------------------------------------------------------
    
	// 저장: HP 파라미터 추가
	virtual void SaveMDFData(const FGuid& ID, const TArray<FMDFHitData>& HitHistory, float CurrentHP) override;

	// 로드: HP 파라미터(Out) 추가
	virtual bool LoadMDFData(const FGuid& ID, TArray<FMDFHitData>& OutHistory, float& OutHP) override;

	// 맵 이동
	UFUNCTION(BlueprintCallable, Category = "GameState")
	void Server_SaveAndMoveLevel(FName NextLevelName);

protected:
	// 디스크 저장 헬퍼 함수
	void WriteDataToDisk();

	// 1. 변형 히스토리 저장소 (메모리)
	TMap<FGuid, TArray<FMDFHitData>> TestSavedMap;

	// 2. [New] 체력 저장소 (메모리)
	TMap<FGuid, float> SavedHPMap;
    
	// 저장 슬롯 이름
	FString SaveSlotName = TEXT("MDF_SaveSlot");
};