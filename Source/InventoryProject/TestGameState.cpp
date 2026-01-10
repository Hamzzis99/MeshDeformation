// Gihyeon's Inventory Project

#include "TestGameState.h"
#include "Kismet/GameplayStatics.h" // Save/Load 함수용

void ATestGameState::BeginPlay()
{
    Super::BeginPlay();

    // 서버만 데이터를 로드하면 됩니다 (변형 정보는 리플리케이션 되므로)
    if (HasAuthority())
    {
        // 1. 저장된 슬롯 확인
        if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
        {
            // 2. 파일 불러오기
            UMDF_SaveActor* LoadedGame = Cast<UMDF_SaveActor>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));

            if (LoadedGame)
            {
                // 3. 파일 내용을 내 메모리(Map)로 복구
                TestSavedMap.Empty(); // 혹시 모를 초기화
                for (const auto& Pair : LoadedGame->SavedDeformationMap)
                {
                    TestSavedMap.Add(Pair.Key, Pair.Value.History);
                }

                UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 새 맵 도착! 데이터 복구 완료 (항목 수: %d)"), TestSavedMap.Num());
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 저장된 데이터가 없습니다. (깨끗한 상태로 시작)"));
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
    // 1. 서버만 실행 가능 (필수)
    if (!HasAuthority()) return;

    // 2. 맵 이름 체크
    if (NextLevelName.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("[TestGameState] 이동할 맵 이름이 없습니다!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 맵 이동 요청(%s). 저장 시작..."), *NextLevelName.ToString());

    // -------------------------------------------------------------------------
    // [저장 로직] (기존 코드 유지)
    // -------------------------------------------------------------------------
    UMDF_SaveActor* SaveInst = Cast<UMDF_SaveActor>(UGameplayStatics::CreateSaveGameObject(UMDF_SaveActor::StaticClass()));
    if (SaveInst)
    {
        // 맵에 있는 데이터를 래퍼로 포장해서 저장 인스턴스에 넣기
        for (const auto& Pair : TestSavedMap)
        {
            FMDFHistoryWrapper Wrapper;
            Wrapper.History = Pair.Value;
            SaveInst->SavedDeformationMap.Add(Pair.Key, Wrapper);
        }

        // 디스크에 기록
        if (UGameplayStatics::SaveGameToSlot(SaveInst, SaveSlotName, 0))
        {
            UE_LOG(LogTemp, Warning, TEXT("[TestGameState] 저장 완료! ServerTravel을 시작합니다."));
        }
    }

    // -------------------------------------------------------------------------
    // [핵심 변경 사항] OpenLevel -> ServerTravel
    // -------------------------------------------------------------------------
    UWorld* World = GetWorld();
    if (World)
    {
        // 1. 이동할 경로 문자열 생성 (예: "/Game/Maps/Stage02")
        // FName을 String으로 변환. 만약 경로 없이 이름만 있다면 경로를 맞춰주는 것이 안전합니다.
        // 보통은 "/Game/Maps/" + NextLevelName.ToString() 처럼 전체 경로를 씁니다.
        FString TravelURL = NextLevelName.ToString();

        // 2. 옵션 추가 (필요시)
        // 데디케이티드 서버에서는 단순히 맵 경로만 주면 됩니다.
        // 리슨 서버라면 "?listen"을 붙여야 하지만, 프로젝트 설정상 데디케이티드이므로 생략 가능.
        
        // 3. 서버 트래블 실행
        // bAbsolute: true면 기존 상태 다 날리고 이동 (일반적), false면 심리스 이동 시도
        // 일단 확실한 이동을 위해 false(기본값) 또는 true 상황에 맞춰 사용.
        // 여기서는 안전하게 절대 이동을 수행합니다.
        World->ServerTravel(TravelURL, false, false); 
    }
}
