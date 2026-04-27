// Copyright Woogle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UClass;
class UObject;
class USimpleConstructionScript;
class USCS_Node;
class UWidgetBlueprint;
class UWidget;
class UWidgetTree;
class UDwBlueprintSnapshotSettings;
class FJsonObject;

/**
 * Blueprint → JSON 스냅샷 생성기.
 * CDO delta, SCS 컴포넌트 트리, 변수 선언, 인터페이스를 추출한다.
 */
class FDwBlueprintSnapshotExporter
{
public:
	/** Blueprint를 스냅샷 JSON으로 추출해 설정된 경로에 기록한다. 실패 시 false 반환 */
	static bool ExportBlueprint(UBlueprint* Blueprint);

	/** PackageName(예: "/Game/UI/Widget/WBP_Ability") 에 대응되는 스냅샷 파일 절대경로. 해결 불가 시 빈 문자열 반환 */
	static FString ResolveSnapshotPath(const FString& PackageName);

	/** ExcludedPropertyNames에 포함된 top-level 프로퍼티는 건너뛴다 (BP NewVariables 중복 제거용). 재귀 subobject에는 적용되지 않는다. */
	static TSharedPtr<FJsonObject> BuildClassDefaults(const UObject* Instance, const UObject* Defaults, const TSet<FName>& ExcludedPropertyNames = TSet<FName>());

private:
	static TSharedRef<FJsonObject> BuildSnapshot(UBlueprint* Blueprint, const UDwBlueprintSnapshotSettings& Settings);

	static TSharedPtr<FJsonObject> BuildComponentsJson(USimpleConstructionScript* SCS);
	static TSharedPtr<FJsonObject> BuildScsNodeJson(USCS_Node* Node);

	static TSharedPtr<FJsonObject> BuildVariablesJson(UBlueprint* Blueprint);
	static TSharedPtr<FJsonObject> BuildInterfacesJson(UBlueprint* Blueprint);

	static TSharedPtr<FJsonObject> BuildWidgetTreeJson(UWidgetTree* WidgetTree);
	static TSharedPtr<FJsonObject> BuildWidgetJson(UWidget* Widget);
	static TSharedPtr<FJsonObject> BuildMvvmJson(UWidgetBlueprint* WidgetBlueprint);

	static TSharedPtr<FJsonValue> BuildEventGraphJson(UBlueprint* Blueprint);
	static TSharedPtr<FJsonObject> BuildFunctionsJson(UBlueprint* Blueprint);

	static FString SerializeJson(TSharedRef<FJsonObject> RootObject);
};
