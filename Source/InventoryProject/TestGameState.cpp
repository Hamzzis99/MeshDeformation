// Gihyeon's Inventory Project
// TestGameState.cpp

#include "TestGameState.h"
#include "Kismet/GameplayStatics.h" 
#include "Engine/World.h"
#include "Save/MDF_SaveActor.h"
#include "MDF_Function/MDF_Instance/MDF_GameInstance.h"

void ATestGameState::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        bool bShouldLoad = false;
        UMDF_GameInstance* GI = Cast<UMDF_GameInstance>(GetGameInstance());
        
        if (GI && GI->bIsMapTransitioning)
        {
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] '맵 이동' 확인증 발견! 데이터를 유지합니다."));
            bShouldLoad = true;
            GI->bIsMapTransitioning = false;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 확인증 없음 (새 게임). 기존 데이터를 파기합니다."));
            if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
            {
                UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);
            }
            bShouldLoad = false;
        }

        if (bShouldLoad)
        {
            if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
            {
                UMDF_SaveActor* LoadedGame = Cast<UMDF_SaveActor>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));

                if (LoadedGame)
                {
                    TestSavedMap.Empty();

                    for (const auto& Pair : LoadedGame->SavedDeformationMap)
                    {
                        // [수정됨] Wrapper 구조체에 데이터를 담아서 맵에 추가
                        FMDFHitHistoryWrapper NewWrapper;
                        NewWrapper.History = Pair.Value.History; // SaveActor의 구조체 구조에 따라 .History로 접근

                        TestSavedMap.Add(Pair.Key, NewWrapper);
                    }

                    UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 데이터 로드 완료! (객체 수: %d)"), TestSavedMap.Num());
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 신규 시작이므로 데이터를 로드하지 않았습니다."));
        }
    }
}

void ATestGameState::SaveMDFData(const FGuid& ID, const TArray<FMDFHitData>& HitHistory)
{
    if (HasAuthority() && ID.IsValid())
    {
       // [수정됨] Wrapper를 만들거나 가져와서 History 갱신
       FMDFHitHistoryWrapper& Wrapper = TestSavedMap.FindOrAdd(ID);
       Wrapper.History = HitHistory;
    }
}

bool ATestGameState::LoadMDFData(const FGuid& ID, TArray<FMDFHitData>& OutHistory)
{
    if (HasAuthority() && ID.IsValid() && TestSavedMap.Contains(ID))
    {
       // [수정됨] Wrapper 안의 History를 꺼내줌
       OutHistory = TestSavedMap[ID].History;
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

    UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 맵 이동 요청(%s). 저장 및 플래그 설정..."), *NextLevelName.ToString());

    WriteDataToDisk();

    UMDF_GameInstance* GI = Cast<UMDF_GameInstance>(GetGameInstance());
    if (GI)
    {
        GI->bIsMapTransitioning = true;
        UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 이사 확인증 발급 완료 (bIsMapTransitioning = true)"));
    }

    UWorld* World = GetWorld();
    if (World)
    {
        FString TravelURL = FString::Printf(TEXT("%s?listen"), *NextLevelName.ToString());
        World->ServerTravel(TravelURL, true, false); 
    }
}

void ATestGameState::WriteDataToDisk()
{
    if (!HasAuthority()) return;

    UMDF_SaveActor* SaveInst = Cast<UMDF_SaveActor>(UGameplayStatics::CreateSaveGameObject(UMDF_SaveActor::StaticClass()));

    if (SaveInst)
    {
        for (const auto& Pair : TestSavedMap)
        {
            FGuid CurrentGUID = Pair.Key;
            
            // SaveActor에 정의된 Wrapper 타입 사용 (이름이 다를 수 있으니 확인 필요, 보통 FMDFHistoryWrapper)
            // 여기서는 기존 코드를 기반으로 작성
            FMDFHistoryWrapper SaveWrapper; 
            SaveWrapper.History = Pair.Value.History; // 내 메모리(Wrapper)에서 꺼내서 넣기

            SaveInst->SavedDeformationMap.Add(CurrentGUID, SaveWrapper);
        }

        if (UGameplayStatics::SaveGameToSlot(SaveInst, SaveSlotName, 0))
        {
            // 저장 성공
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[TestGameState] 디스크 저장 실패!"));
        }
    }
}