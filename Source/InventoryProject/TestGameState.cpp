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

    // 서버(Authority)에서만 데이터 관리 수행
    if (HasAuthority())
    {
        bool bShouldLoad = false;

        // 1. 게임 인스턴스를 통해 "맵 이동 중"인지 확인
        UMDF_GameInstance* GI = Cast<UMDF_GameInstance>(GetGameInstance());
        
        if (GI && GI->bIsMapTransitioning)
        {
            // Case A: 맵 이동으로 넘어옴 -> 데이터 로드 허용
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] '맵 이동' 확인증 발견! 데이터를 유지합니다."));
            bShouldLoad = true;

            // 확인증 회수 (다음 번 로드 방지 및 상태 초기화)
            GI->bIsMapTransitioning = false;
        }
        else
        {
            // Case B: 새 게임 혹은 강제 종료 후 재시작 -> 데이터 초기화
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 확인증 없음 (새 게임). 기존 데이터를 파기합니다."));
            
            if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
            {
                // 기존 세이브 파일 삭제
                UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);
                UE_LOG(LogTemp, Error, TEXT("[TestGameState] 기존 세이브 파일 삭제 완료!"));
            }
            
            bShouldLoad = false;
        }

        // 2. 로드 허용 시에만 파일 읽기 수행
        if (bShouldLoad)
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
        }
        else
        {
            // 로드하지 않음 (신규 시작)
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 신규 시작이므로 데이터를 로드하지 않았습니다."));
        }
    }
}

void ATestGameState::SaveMDFData(const FGuid& ID, const TArray<FMDFHitData>& HitHistory, float CurrentHP)
{
    if (HasAuthority() && ID.IsValid())
    {
       // 1. 히스토리 맵 갱신 (RAM)
       TestSavedMap.Add(ID, HitHistory);
       
       // 2. HP 맵 갱신 (RAM)
       SavedHPMap.Add(ID, CurrentHP);
       
       // [최적화] WriteDataToDisk() 삭제함
       // 이제 총알을 맞을 때마다 디스크에 쓰지 않고, 메모리(RAM)만 갱신합니다.
       // 실제 저장은 맵 이동(MoveActor) 시에만 발생합니다.
       // UE_LOG(LogTemp, Log, TEXT("[TestGameState] 메모리 갱신됨 (ID: %s / HP: %.1f)."), *ID.ToString(), CurrentHP);
    }
}

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
    // [선택 사항] 맵 이동 시 이미 저장을 수행하므로 여기서는 굳이 안 해도 됩니다.
    // 비상용(강제 종료 대비)으로 남겨두거나, 중복 저장이 싫다면 주석 처리해도 됩니다.
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

    // 1. 이동 전 데이터 저장 (필수 - 여기가 유일한 저장 타이밍)
    WriteDataToDisk();

    // 2. ★ GameInstance에 "나 이사 간다!" 표시 (핵심)
    UMDF_GameInstance* GI = Cast<UMDF_GameInstance>(GetGameInstance());
    if (GI)
    {
        GI->bIsMapTransitioning = true;
        UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 이사 확인증 발급 완료 (bIsMapTransitioning = true)"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("[TestGameState] GameInstance 형변환 실패! 프로젝트 세팅을 확인하세요."));
    }

    // 3. ServerTravel 실행
    UWorld* World = GetWorld();
    if (World)
    {
        // ?listen 옵션을 붙여야 클라이언트들이 접속 가능
        FString TravelURL = FString::Printf(TEXT("%s?listen"), *NextLevelName.ToString());
        
        // bAbsolute=true(완전 이동), bShouldSkipGameNotify=false
        World->ServerTravel(TravelURL, true, false); 
    }
}

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
            // 저장 성공
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[TestGameState] 디스크 저장 실패!"));
        }
    }
}