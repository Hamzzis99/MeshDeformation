// Gihyeon's Inventory Project

#include "TestGameState.h"
#include "Kismet/GameplayStatics.h" // Save/Load 함수용

void ATestGameState::BeginPlay()
{
    Super::BeginPlay();

    // 1. 디스크에 저장된 파일이 있는지 확인
    if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
    {
        // 2. 파일 불러오기
        UMDF_SaveActor* LoadedGame = Cast<UMDF_SaveActor>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));

        if (LoadedGame)
        {
            // 3. 금고(Wrapper)에서 꺼내서 내 장부(TMap)에 기록
            // (Wrapper를 벗겨내는 과정)
            for (const auto& Pair : LoadedGame->SavedDeformationMap)
            {
                TestSavedMap.Add(Pair.Key, Pair.Value.History);
            }

            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 디스크에서 데이터 복원 완료! (복구된 객체 수: %d)"), TestSavedMap.Num());
        }
    }
}

void ATestGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 클라이언트는 저장 금지! (서버의 데이터를 0으로 덮어쓸 위험 차단)
    if (!HasAuthority())
    {
        Super::EndPlay(EndPlayReason);
        return;
    }
    
    // 1. 빈 금고(SaveGame 객체) 생성
    UMDF_SaveActor* SaveInst = Cast<UMDF_SaveActor>(UGameplayStatics::CreateSaveGameObject(UMDF_SaveActor::StaticClass()));

    if (SaveInst)
    {
        // 2. 내 장부의 데이터를 금고에 옮겨 담기
        // (Wrapper로 포장하는 과정)
        for (const auto& Pair : TestSavedMap)
        {
            FMDFHistoryWrapper Wrapper;
            Wrapper.History = Pair.Value;
            SaveInst->SavedDeformationMap.Add(Pair.Key, Wrapper);
        }

        // 3. 디스크에 쓰기 (저장)
        if (UGameplayStatics::SaveGameToSlot(SaveInst, SaveSlotName, 0))
        {
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 디스크에 저장 성공! (저장된 객체 수: %d)"), TestSavedMap.Num());
        }
    }

    Super::EndPlay(EndPlayReason);
}

// [MDF Interface 구현]
void ATestGameState::SaveMDFData(const FGuid& ID, const TArray<FMDFHitData>& Data)
{
    if (HasAuthority() && ID.IsValid())
    {
       TestSavedMap.Add(ID, Data);
       UE_LOG(LogTemp, Log, TEXT("[TestGameState] (메모리) 데이터 갱신됨 ID: %s"), *ID.ToString());
    }
}

bool ATestGameState::LoadMDFData(const FGuid& ID, TArray<FMDFHitData>& OutData)
{
    if (HasAuthority() && ID.IsValid() && TestSavedMap.Contains(ID))
    {
       OutData = TestSavedMap[ID];
       return true;
    }
    return false;
}

void ATestGameState::Server_SaveAndMoveLevel(FName NextLevelName)
{
    // 1. 서버만 실행 가능
    if (!HasAuthority()) return;

    // 2. 맵 이름 체크
    if (NextLevelName.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("[TestGameState] 이동할 맵 이름이 없습니다!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 맵 이동 요청(%s). 저장 시작..."), *NextLevelName.ToString());

    // 3. 강제 저장 수행
    UMDF_SaveActor* SaveInst = Cast<UMDF_SaveActor>(UGameplayStatics::CreateSaveGameObject(UMDF_SaveActor::StaticClass()));
    if (SaveInst)
    {
        for (const auto& Pair : TestSavedMap)
        {
            FMDFHistoryWrapper Wrapper;
            Wrapper.History = Pair.Value;
            SaveInst->SavedDeformationMap.Add(Pair.Key, Wrapper);
        }

        if (UGameplayStatics::SaveGameToSlot(SaveInst, SaveSlotName, 0))
        {
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 저장 완료! 이동합니다."));
        }
    }

    // 4. 모두 데리고 이동
    UGameplayStatics::OpenLevel(this, NextLevelName);
}
