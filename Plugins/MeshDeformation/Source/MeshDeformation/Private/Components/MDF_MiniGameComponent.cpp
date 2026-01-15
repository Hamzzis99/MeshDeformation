// Gihyeon's MeshDeformation Project

#include "Components/MDF_MiniGameComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"

// [Geometry Script 헤더] 버텍스 선택 및 색칠용
#include "GeometryScript/MeshSelectionFunctions.h"
#include "GeometryScript/MeshVertexColorFunctions.h"

UMDF_MiniGameComponent::UMDF_MiniGameComponent()
{
	// 미니게임 벽은 기본적으로 업데이트 빈도가 높을 수 있음
}

void UMDF_MiniGameComponent::PaintWeakness(FVector WorldLocation, float Radius)
{
	// 1. 서버 권한 확인 (멀티플레이) -> 색칠도 서버가 해서 뿌려줘야 함
	if (!GetOwner()->HasAuthority()) return;

	// 2. 월드 좌표 -> 로컬 좌표 변환 (부모 클래스 함수 활용)
	FVector LocalLoc = ConvertWorldToLocal(WorldLocation);

	// 3. 색칠 로직 실행
	ApplyVertexColorPaint(LocalLoc, Radius);

	// 4. (중요) 리플리케이션이 필요하지만, 
	// Vertex Color는 데이터량이 많아 NetMulticast로 모든 클라에게 "여기 칠해!"라고 명령하는 게 낫습니다.
	// 일단은 서버에서만 칠해보고, 잘 되면 Multicast 추가하겠습니다.
}

void UMDF_MiniGameComponent::ApplyVertexColorPaint(FVector LocalLocation, float Radius)
{
	UDynamicMeshComponent* DynMeshComp = GetDynamicMeshComponent(); // 부모에 Getter가 없다면 직접 접근 필요
	if (!DynMeshComp) return;

	UDynamicMesh* TargetMesh = DynMeshComp->GetDynamicMesh();
	if (!TargetMesh) return;

	// [Geometry Script] 1. 칠할 영역 선택 (구체 범위)
	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectMeshElementsInSphere(
		TargetMesh,
		EGeometryScriptMeshSelectionType::Vertices,
		LocalLocation,
		Radius,
		Selection
	);

	// [Geometry Script] 2. 선택된 버텍스를 빨간색(Red)으로 변경
	// LinearColor(1, 0, 0, 1) = Red
	UGeometryScriptLibrary_MeshVertexColorFunctions::SetMeshSelectionVertexColor(
		TargetMesh,
		Selection,
		FLinearColor::Red, 
		FGeometryScriptColorFlags() // 기본값 (R,G,B,A 모두 덮어쓰기)
	);
}

void UMDF_MiniGameComponent::TryBreach(const FHitResult& HitInfo, float DamageAmount)
{
	// 아직 구현 안 함 (다음 단계에서 슈터 무기 만들 때 구현)
	// 일단 로그만 찍기
	UE_LOG(LogTemp, Log, TEXT("[MiniGame] 총알이 맞았습니다!"));
}