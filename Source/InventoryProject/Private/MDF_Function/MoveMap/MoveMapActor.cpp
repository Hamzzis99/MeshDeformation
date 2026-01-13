// Gihyeon's Inventory Project

#include "MDF_Function/MoveMap/MoveMapActor.h"
#include "Kismet/GameplayStatics.h"
#include "TestGameState.h"

AMoveMapActor::AMoveMapActor()
{
    PrimaryActorTick.bCanEverTick = false; 
    bReplicates = true; 

    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    RootComponent = MeshComp;
}

void AMoveMapActor::BeginPlay()
{
    Super::BeginPlay();
}

// [인터페이스 구현]
bool AMoveMapActor::ExecuteInteract_Implementation(APlayerController* Controller)
{
    Interact();
    return true; 
}

void AMoveMapActor::Interact()
{
    // 1. 서버 권한 확인
    if (!HasAuthority()) 
    {
        return;
    }

    // 2. 맵 이름 체크
    if (NextLevelName.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("[MoveMapActor] 이동할 맵 이름이 없습니다!"));
        return;
    }

    // 3. GameState 가져오기
    AGameStateBase* GS = UGameplayStatics::GetGameState(this);
    ATestGameState* MyGS = Cast<ATestGameState>(GS);

    if (MyGS)
    {
        // ★ 핵심: GameState야, 데이터 저장 좀 하고 맵 이동 시켜줘!
        UE_LOG(LogTemp, Log, TEXT("[MoveMapActor] GameState에게 저장 및 이동 요청: %s"), *NextLevelName.ToString());
       
        // 이 함수 안에서 SaveLogic() -> ServerTravel() 순서로 진행되어야 함
        MyGS->Server_SaveAndMoveLevel(NextLevelName);
    }
    else
    {
        // 만약 Cast 실패 시 비상 대책으로 직접 이동 (혹은 에러 로그)
        UE_LOG(LogTemp, Error, TEXT("[MoveMapActor] TestGameState를 찾을 수 없습니다! 캐스팅 실패."));
    }
}