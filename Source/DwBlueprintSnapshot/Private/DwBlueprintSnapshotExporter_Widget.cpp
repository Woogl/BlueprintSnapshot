// Copyright Woogle. All Rights Reserved.

//
// Widget Blueprint의 WidgetTree 계층과 위젯별 delta(CDO 차이, PanelSlot 값)를 추출한다.
// MVVM 관련은 DwBlueprintSnapshotExporter_MVVM.cpp 참조.
//

#include "DwBlueprintSnapshotExporter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/PanelSlot.h"
#include "UObject/Class.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

TSharedPtr<FJsonObject> FDwBlueprintSnapshotExporter::BuildWidgetTreeJson(UWidgetTree* WidgetTree)
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

	if (UWidget* RootWidget = WidgetTree->RootWidget)
	{
		Root->SetStringField(TEXT("root"), RootWidget->GetName());
	}

	TSharedPtr<FJsonObject> WidgetsMap = MakeShared<FJsonObject>();
	WidgetTree->ForEachWidget([&WidgetsMap](UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}
		TSharedPtr<FJsonObject> WidgetJson = FDwBlueprintSnapshotExporter::BuildWidgetJson(Widget);
		if (!WidgetJson.IsValid())
		{
			return;
		}
		FString Key = Widget->GetName();
		if (WidgetsMap->HasField(Key))
		{
			int32 Suffix = 2;
			FString Unique;
			do
			{
				Unique = FString::Printf(TEXT("%s#%d"), *Key, Suffix++);
			}
			while (WidgetsMap->HasField(Unique));
			Key = MoveTemp(Unique);
		}
		WidgetsMap->SetObjectField(Key, WidgetJson.ToSharedRef());
	});

	if (WidgetsMap->Values.Num() == 0)
	{
		return nullptr;
	}
	Root->SetObjectField(TEXT("widgets"), WidgetsMap.ToSharedRef());

	return Root;
}

TSharedPtr<FJsonObject> FDwBlueprintSnapshotExporter::BuildWidgetJson(UWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("class"), Widget->GetClass()->GetPathName());

	if (Widget->bIsVariable)
	{
		Json->SetBoolField(TEXT("isVariable"), true);
	}

	if (UWidget* Parent = Widget->GetParent())
	{
		Json->SetStringField(TEXT("parent"), Parent->GetName());
	}

	if (UPanelSlot* Slot = Widget->Slot)
	{
		TSharedPtr<FJsonObject> SlotJson = MakeShared<FJsonObject>();
		SlotJson->SetStringField(TEXT("class"), Slot->GetClass()->GetPathName());
		UObject* SlotDefaults = Slot->GetClass()->GetDefaultObject(false);
		TSharedPtr<FJsonObject> SlotDelta = BuildClassDefaults(Slot, SlotDefaults);
		if (SlotDelta.IsValid() && SlotDelta->Values.Num() > 0)
		{
			SlotJson->SetObjectField(TEXT("delta"), SlotDelta.ToSharedRef());
		}
		Json->SetObjectField(TEXT("slot"), SlotJson.ToSharedRef());
	}

	UObject* ClassDefaults = Widget->GetClass()->GetDefaultObject(false);
	TSharedPtr<FJsonObject> Delta = BuildClassDefaults(Widget, ClassDefaults);
	if (Delta.IsValid())
	{
		// Widget의 delta 안에 있는 "Slot" 필드는 WidgetTree 내부 경로 문자열로,
		// 최상위 slot/parent 정보와 중복되므로 드롭한다.
		Delta->RemoveField(TEXT("Slot"));
		if (Delta->Values.Num() > 0)
		{
			Json->SetObjectField(TEXT("delta"), Delta.ToSharedRef());
		}
	}

	return Json;
}
