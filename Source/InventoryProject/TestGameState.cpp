// Gihyeon's Inventory Project
// TestGameState.cpp

#include "TestGameState.h"
#include "Kismet/GameplayStatics.h" 
#include "Engine/World.h"

// [필수] SaveGame 클래스 (경로가 Plugins라면 맞춰주세요. 예: "MDF/Public/Save/MDF_SaveActor.h")
// 만약 못 찾으면 프로젝트 구성에 따라 "MDF_SaveActor.h" 로 시도해보세요.
#include "Save/MDF_SaveActor.h" 

void ATestGameState::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
        {
            UMDF_SaveActor* LoadedGame = Cast<UMDF_SaveActor>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));

            if (LoadedGame)
            {
                // 메모리 초기화
                TestSavedMap.Empty();
                SavedHPMap.Empty();

                // [로드] 파일 내용을 내 메모리(Map 2개)로 복구
                for (const auto& Pair : LoadedGame->SavedDeformationMap)
                {
                    // 1. 히스토리 복구
                    TestSavedMap.Add(Pair.Key, Pair.Value.History);
                    
                    // 2. HP 복구
                    SavedHPMap.Add(Pair.Key, Pair.Value.SavedHP);
                }

                UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 데이터 로드 완료! (객체 수: %d)"), TestSavedMap.Num());
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 저장된 데이터 없음 (신규 시작)."));
        }
    }
}

// [수정됨] 인터페이스 구현: HP 파라미터 추가
void ATestGameState::SaveMDFData(const FGuid& ID, const TArray<FMDFHitData>& HitHistory, float CurrentHP)
{
    if (HasAuthority() && ID.IsValid())
    {
       // 1. 히스토리 맵 갱신
       TestSavedMap.Add(ID, HitHistory);
       
       // 2. HP 맵 갱신
       SavedHPMap.Add(ID, CurrentHP);
       
       UE_LOG(LogTemp, Log, TEXT("[TestGameState] 메모리 갱신됨 (ID: %s / HP: %.1f). 저장 시도..."), *ID.ToString(), CurrentHP);

       // 3. 즉시 디스크 저장
       WriteDataToDisk();
    }
}

// [수정됨] 인터페이스 구현: HP 파라미터 추가
bool ATestGameState::LoadMDFData(const FGuid& ID, TArray<FMDFHitData>& OutHistory, float& OutHP)
{
    if (HasAuthority() && ID.IsValid() && TestSavedMap.Contains(ID))
    {
       // 1. 히스토리 반환
       OutHistory = TestSavedMap[ID];
       
       // 2. HP 반환 (저장된 게 없으면 기본값 1000)
       if (SavedHPMap.Contains(ID))
       {
           OutHP = SavedHPMap[ID];
       }
       else
       {
           OutHP = 1000.0f; 
       }
       return true;
    }
    return false;
}

void ATestGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        WriteDataToDisk();
    }
    Super::EndPlay(EndPlayReason);
}

void ATestGameState::Server_SaveAndMoveLevel(FName NextLevelName)
{
    if (!HasAuthority()) return;

    if (NextLevelName.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("[TestGameState] 이동할 맵 이름이 없습니다!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 맵 이동 요청(%s). 최종 저장 수행..."), *NextLevelName.ToString());

    // 1. 이동 전 저장
    WriteDataToDisk();

    // 2. ServerTravel
    UWorld* World = GetWorld();
    if (World)
    {
        FString TravelURL = NextLevelName.ToString();
        World->ServerTravel(TravelURL, false, false); 
    }
}

// [Helper Function]
void ATestGameState::WriteDataToDisk()
{
    if (!HasAuthority()) return;

    UMDF_SaveActor* SaveInst = Cast<UMDF_SaveActor>(UGameplayStatics::CreateSaveGameObject(UMDF_SaveActor::StaticClass()));

    if (SaveInst)
    {
        // 메모리(Map 2개) -> 세이브 객체(Wrapper)로 합치기
        for (const auto& Pair : TestSavedMap)
        {
            FGuid CurrentGUID = Pair.Key;
            
            FMDFHistoryWrapper Wrapper;
            Wrapper.History = Pair.Value; // 히스토리 넣기

            // HP 찾아서 넣기
            if (SavedHPMap.Contains(CurrentGUID))
            {
                Wrapper.SavedHP = SavedHPMap[CurrentGUID];
            }
            else
            {
                Wrapper.SavedHP = 1000.0f; // 기본값
            }

            SaveInst->SavedDeformationMap.Add(CurrentGUID, Wrapper);
        }

        // 파일로 저장
        if (UGameplayStatics::SaveGameToSlot(SaveInst, SaveSlotName, 0))
        {
            // 성공 로그 (필요 시 주석 해제)
            // UE_LOG(LogTemp, Log, TEXT("[TestGameState] 디스크 저장 완료."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[TestGameState] 디스크 저장 실패!"));
        }
    }
}