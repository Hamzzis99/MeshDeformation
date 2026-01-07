// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class InventoryProjectTarget : TargetRules
{
	public InventoryProjectTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		
		// 5.7 버전에 맞는 최신 빌드 설정 적용
		DefaultBuildSettings = BuildSettingsVersion.Latest; 
		
		// Include 순서를 5.7 기준으로 설정하여 컴파일 경고 및 오류 방지
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		
		ExtraModuleNames.Add("InventoryProject");
	}
}