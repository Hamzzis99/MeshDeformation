// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class InventoryProjectServerTarget : TargetRules
{
	public InventoryProjectServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		
		// 5.7 버전에 최적화된 빌드 설정 적용
		DefaultBuildSettings = BuildSettingsVersion.V6;
		
		// 엔진 포함 순서를 5.7 버전 기준으로 업데이트
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		
		ExtraModuleNames.Add("InventoryProject");
	}
}