// Gihyeon's Inventory Project

#include "Public/TestGameState.h"

void ATestGameState::SaveMDFData(const FGuid& ID, const TArray<FMDFHitData>& Data)
{
	// 서버에서만 저장
	if (HasAuthority() && ID.IsValid())
	{
		TestSavedMap.Add(ID, Data);
		UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 데이터 저장됨! ID: %s / 개수: %d"), *ID.ToString(), Data.Num());
	}
}

bool ATestGameState::LoadMDFData(const FGuid& ID, TArray<FMDFHitData>& OutData)
{
	// 서버에서만 로드
	if (HasAuthority() && ID.IsValid() && TestSavedMap.Contains(ID))
	{
		OutData = TestSavedMap[ID];
		UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 데이터 불러옴! ID: %s"), *ID.ToString());
		return true;
	}
	return false;
}