// Gihyeon's Deformation Project (Helluna)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Components/MDF_DeformableComponent.h"
#include "MDF_SaveGame.generated.h"

/** * [저장 단위] 벽 하나에 대한 저장 데이터 */
USTRUCT()
struct FMDFWallSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMDFHitData> HitHistory; // 벽 하나가 가진 타격 히스토리를 저장
};

/** * [저장소 클래스] 실제 .sav 파일로 저장될 객체 */
UCLASS()
class MESHDEFORMATION_API UMDF_SaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** 핵심 저장소: (벽의 GUID) -> (그 벽의 데이터) */
	UPROPERTY()
	TMap<FGuid, FMDFWallSaveData> SavedWalls;
};