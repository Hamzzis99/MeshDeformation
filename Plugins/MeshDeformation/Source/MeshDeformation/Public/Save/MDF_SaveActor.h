// Gihyeon's Deformation Project (Helluna)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Components/MDF_DeformableComponent.h"
#include "MDF_SaveActor.generated.h"

/**
 * [기술적 이유]
 * UPROPERTY는 TMap<Key, TArray<Value>> 형태의 이중 컨테이너 저장을 지원하지 않습니다.
 * 따라서 TArray를 감싸는 래퍼(Wrapper) 구조체가 필요합니다.
 */
USTRUCT()
struct FMDFHistoryWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMDFHitData> History;
};

/**
 * [Step 11] 플러그인 전용 세이브 게임 클래스
 * 게임 모드가 바뀌거나(OpenLevel), 게임을 껐다 켤 때 데이터를 디스크에 보관하는 금고입니다.
 */
UCLASS()
class MESHDEFORMATION_API UMDF_SaveActor : public USaveGame
{
	GENERATED_BODY()

public:
	/** * 저장된 변형 데이터 장부
	 * Key: 컴포넌트 GUID (신분증)
	 * Value: 히스토리 데이터 (포장된 형태)
	 */
	UPROPERTY(VisibleAnywhere, Category = "MDF Save")
	TMap<FGuid, FMDFHistoryWrapper> SavedDeformationMap;
};
