// Gihyeon's Inventory Project
#include "Public/MoveMap/MoveMapActor.h"

#include "Kismet/GameplayStatics.h"
#include "TestGameState.h" // 같은 프로젝트 식구니까 include 가능!

AMoveMapActor::AMoveMapActor()
{
	PrimaryActorTick.bCanEverTick = false; // 틱 필요 없음
	bReplicates = true; // 서버 통신용

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;
}

void AMoveMapActor::BeginPlay()
{
	Super::BeginPlay();
}

void AMoveMapActor::Interact()
{
	// 서버로 요청
	Server_RequestMove();
}

void AMoveMapActor::Server_RequestMove_Implementation()
{
	// 1. GameState 가져오기
	AGameStateBase* GS = UGameplayStatics::GetGameState(this);
	ATestGameState* MyGS = Cast<ATestGameState>(GS);

	// 2. 저장하고 이동해라!
	if (MyGS)
	{
		MyGS->Server_SaveAndMoveLevel(NextLevelName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MoveMapActor] TestGameState를 찾을 수 없습니다!"));
	}
}