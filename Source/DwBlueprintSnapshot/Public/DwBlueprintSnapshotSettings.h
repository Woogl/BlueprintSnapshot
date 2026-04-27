// Copyright Woogle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "DwBlueprintSnapshotSettings.generated.h"

/**
 * 블루프린트를 저장할 때마다 내용을 JSON 파일로 기록하는 플러그인의 설정.
 * 저장된 JSON은 diff로 변경점을 읽거나 AI에게 블루프린트 구조를 전달할 때 사용 가능.
 */
UCLASS(Config = Editor, Meta = (DisplayName = "Dw Blueprint Snapshot"))
class DWBLUEPRINTSNAPSHOT_API UDwBlueprintSnapshotSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDwBlueprintSnapshotSettings();

	/** 전체 기능 on/off */
	UPROPERTY(EditAnywhere, Config, Category = "General")
	bool bEnabled = true;

	/** 스냅샷 파일 확장자. 점을 포함해 입력 (예: .json, .bpj) */
	UPROPERTY(EditAnywhere, Config, Category = "Output")
	FString FileExtension = TEXT(".json");

	/** 스냅샷을 저장할 폴더 */
	UPROPERTY(EditAnywhere, Config, Category = "Output")
	FDirectoryPath OutputDirectory;

	/** 대상 BP 폴더. 비어있으면 모든 BP가 대상 */
	UPROPERTY(EditAnywhere, Config, Category = "Filter", Meta = (ContentDir, LongPackageName))
	TArray<FDirectoryPath> IncludeDirectories;

	/** 제외할 BP 폴더 */
	UPROPERTY(EditAnywhere, Config, Category = "Filter", Meta = (ContentDir, LongPackageName))
	TArray<FDirectoryPath> ExcludeDirectories;

	/** 기본값과 동일한 프로퍼티는 건너뜀 */
	UPROPERTY(EditAnywhere, Config, Category = "Scope")
	bool bSkipUnchangedDefaults = true;

	/** SimpleConstructionScript 컴포넌트 트리 포함 */
	UPROPERTY(EditAnywhere, Config, Category = "Scope")
	bool bIncludeComponents = true;

	/** BP NewVariables 포함 */
	UPROPERTY(EditAnywhere, Config, Category = "Scope")
	bool bIncludeVariables = true;

	/** Event Graph / Function Graph를 의사 코드 문자열로 포함 */
	UPROPERTY(EditAnywhere, Config, Category = "Scope")
	bool bIncludeGraphs = true;

	/** ImplementedInterfaces 포함 */
	UPROPERTY(EditAnywhere, Config, Category = "Scope")
	bool bIncludeInterfaces = true;

	/** WBP의 WidgetTree(위젯 계층 + 각 위젯의 CDO) 포함 */
	UPROPERTY(EditAnywhere, Config, Category = "Scope")
	bool bIncludeWidgetTree = true;

	/** WBP의 MVVM View 확장 (ViewModel 컨텍스트 + 바인딩) 포함 */
	UPROPERTY(EditAnywhere, Config, Category = "Scope")
	bool bIncludeMVVM = true;
};
