// Copyright Woogle. All Rights Reserved.

#include "DwBlueprintSnapshotExporter.h"
#include "DwBlueprintSnapshotModule.h"
#include "DwBlueprintSnapshotSettings.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "WidgetBlueprint.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"

namespace
{
	void SortJsonValueRecursive(const TSharedPtr<FJsonValue>& Value);

	void SortJsonObjectRecursive(TSharedPtr<FJsonObject> Obj)
	{
		if (!Obj.IsValid())
		{
			return;
		}
		Obj->Values.KeySort(TLess<FString>());
		for (auto& Pair : Obj->Values)
		{
			SortJsonValueRecursive(Pair.Value);
		}
	}

	void SortJsonValueRecursive(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return;
		}
		if (Value->Type == EJson::Object)
		{
			SortJsonObjectRecursive(Value->AsObject());
			return;
		}
		if (Value->Type == EJson::Array)
		{
			for (const TSharedPtr<FJsonValue>& Elem : Value->AsArray())
			{
				SortJsonValueRecursive(Elem);
			}
		}
	}

	void SetObjectFieldIfNonEmpty(FJsonObject& Root, const FString& Key, const TSharedPtr<FJsonObject>& Value)
	{
		if (Value.IsValid() && Value->Values.Num() > 0)
		{
			Root.SetObjectField(Key, Value.ToSharedRef());
		}
	}
}

bool FDwBlueprintSnapshotExporter::ExportBlueprint(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return false;
	}

	const UDwBlueprintSnapshotSettings* Settings = GetDefault<UDwBlueprintSnapshotSettings>();
	if (!Settings)
	{
		return false;
	}

	TSharedRef<FJsonObject> Root = BuildSnapshot(Blueprint, *Settings);

	UPackage* Package = Blueprint->GetOutermost();
	FString LatestPath = Package ? ResolveSnapshotPath(Package->GetName()) : FString();
	if (LatestPath.IsEmpty())
	{
		return false;
	}

	// Windows MAX_PATH 가드: 경로가 과도하게 길면 SaveStringToFile이 실패하므로, 조용히 폴백하지 않고 명시적으로 스킵한다.
	const int32 MaxPathLen = 240;
	if (LatestPath.Len() > MaxPathLen)
	{
		UE_LOG(LogDwBPSnapshot, Error,
			TEXT("Snapshot path exceeds %d chars (len=%d), skipping. BP=%s  Path=%s"),
			MaxPathLen, LatestPath.Len(), *Blueprint->GetPathName(), *LatestPath);
		return false;
	}

	IFileManager& FileManager = IFileManager::Get();
	FileManager.MakeDirectory(*FPaths::GetPath(LatestPath), true);

	const FString Json = SerializeJson(Root);

	// Read-only (SCC 추적 등) 인 경우 해제 시도. 저장 실패하면 원상 복구.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	bool bClearedReadOnly = false;
	if (PlatformFile.FileExists(*LatestPath) && PlatformFile.IsReadOnly(*LatestPath))
	{
		PlatformFile.SetReadOnly(*LatestPath, false);
		bClearedReadOnly = true;
	}

	if (!FFileHelper::SaveStringToFile(Json, *LatestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		if (bClearedReadOnly)
		{
			PlatformFile.SetReadOnly(*LatestPath, true);
		}
		UE_LOG(LogDwBPSnapshot, Warning, TEXT("Failed to write %s"), *LatestPath);
		return false;
	}

	return true;
}

TSharedRef<FJsonObject> FDwBlueprintSnapshotExporter::BuildSnapshot(UBlueprint* Blueprint, const UDwBlueprintSnapshotSettings& Settings)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
	Root->SetStringField(TEXT("parentClass"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : TEXT(""));

	UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	if (GeneratedClass && Blueprint->ParentClass)
	{
		UObject* InstanceCDO = GeneratedClass->GetDefaultObject(false);
		UObject* ParentCDO = Blueprint->ParentClass->GetDefaultObject(false);
		if (InstanceCDO && ParentCDO)
		{
			// NewVariables / SCS 컴포넌트는 각각 `newVariables` / `components` 필드에 기록되므로 classDefaults 델타에서 중복 제외.
			TSet<FName> ExcludedNames;
			ExcludedNames.Reserve(Blueprint->NewVariables.Num());
			for (const FBPVariableDescription& Var : Blueprint->NewVariables)
			{
				ExcludedNames.Add(Var.VarName);
			}
			if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
			{
				for (USCS_Node* Node : SCS->GetAllNodes())
				{
					if (Node)
					{
						ExcludedNames.Add(Node->GetVariableName());
					}
				}
			}
			SetObjectFieldIfNonEmpty(*Root, TEXT("classDefaults"), BuildClassDefaults(InstanceCDO, ParentCDO, ExcludedNames));
		}
	}

	if (Settings.bIncludeComponents)
	{
		if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript)
		{
			SetObjectFieldIfNonEmpty(*Root, TEXT("components"), BuildComponentsJson(SCS));
		}
	}

	if (Settings.bIncludeVariables)
	{
		SetObjectFieldIfNonEmpty(*Root, TEXT("newVariables"), BuildVariablesJson(Blueprint));
	}

	if (Settings.bIncludeInterfaces)
	{
		SetObjectFieldIfNonEmpty(*Root, TEXT("interfaces"), BuildInterfacesJson(Blueprint));
	}

	if (Settings.bIncludeGraphs)
	{
		if (TSharedPtr<FJsonValue> EventGraph = BuildEventGraphJson(Blueprint))
		{
			Root->SetField(TEXT("eventGraph"), EventGraph);
		}
		SetObjectFieldIfNonEmpty(*Root, TEXT("newFunctions"), BuildFunctionsJson(Blueprint));
	}

	if (UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Blueprint))
	{
		if (Settings.bIncludeWidgetTree)
		{
			SetObjectFieldIfNonEmpty(*Root, TEXT("widgetTree"), BuildWidgetTreeJson(WidgetBlueprint->WidgetTree));
		}

		if (Settings.bIncludeMVVM)
		{
			SetObjectFieldIfNonEmpty(*Root, TEXT("mvvm"), BuildMvvmJson(WidgetBlueprint));
		}
	}

	return Root;
}

TSharedPtr<FJsonObject> FDwBlueprintSnapshotExporter::BuildComponentsJson(USimpleConstructionScript* SCS)
{
	if (!SCS)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	for (USCS_Node* Node : SCS->GetAllNodes())
	{
		TSharedPtr<FJsonObject> NodeJson = BuildScsNodeJson(Node);
		if (NodeJson.IsValid())
		{
			Root->SetObjectField(Node->GetVariableName().ToString(), NodeJson.ToSharedRef());
		}
	}
	return Root;
}

TSharedPtr<FJsonObject> FDwBlueprintSnapshotExporter::BuildScsNodeJson(USCS_Node* Node)
{
	if (!Node)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> NodeJson = MakeShared<FJsonObject>();
	NodeJson->SetStringField(TEXT("variableName"), Node->GetVariableName().ToString());

	UActorComponent* Template = Node->ComponentTemplate;
	if (Template)
	{
		NodeJson->SetStringField(TEXT("componentClass"), Template->GetClass()->GetPathName());

		UObject* ClassDefaults = Template->GetClass()->GetDefaultObject(false);
		TSharedPtr<FJsonObject> TemplateDelta = BuildClassDefaults(Template, ClassDefaults);
		if (TemplateDelta.IsValid() && TemplateDelta->Values.Num() > 0)
		{
			NodeJson->SetObjectField(TEXT("delta"), TemplateDelta.ToSharedRef());
		}
	}

	if (USimpleConstructionScript* SCS = Node->GetSCS())
	{
		if (USCS_Node* Parent = SCS->FindParentNode(Node))
		{
			NodeJson->SetStringField(TEXT("attachParent"), Parent->GetVariableName().ToString());
		}
	}

	if (!Node->AttachToName.IsNone())
	{
		NodeJson->SetStringField(TEXT("attachSocket"), Node->AttachToName.ToString());
	}

	return NodeJson;
}

TSharedPtr<FJsonObject> FDwBlueprintSnapshotExporter::BuildVariablesJson(UBlueprint* Blueprint)
{
	auto FormatTerminal = [](const FName& Category, const TWeakObjectPtr<UObject>& SubCatObj) -> FString
	{
		if (UObject* Obj = SubCatObj.Get())
		{
			return Obj->GetName();
		}
		return Category.ToString();
	};

	UBlueprintGeneratedClass* GeneratedClass = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
	UObject* CDO = GeneratedClass ? GeneratedClass->GetDefaultObject(false) : nullptr;

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	for (const FBPVariableDescription& Var : Blueprint->NewVariables)
	{
		FString TypeStr = FormatTerminal(Var.VarType.PinCategory, Var.VarType.PinSubCategoryObject);

		if (Var.VarType.IsArray())
		{
			TypeStr = FString::Printf(TEXT("%s[]"), *TypeStr);
		}
		else if (Var.VarType.IsSet())
		{
			TypeStr = FString::Printf(TEXT("Set<%s>"), *TypeStr);
		}
		else if (Var.VarType.IsMap())
		{
			const FString ValueStr = FormatTerminal(Var.VarType.PinValueType.TerminalCategory, Var.VarType.PinValueType.TerminalSubCategoryObject);
			TypeStr = FString::Printf(TEXT("Map<%s, %s>"), *TypeStr, *ValueStr);
		}

		// CDO에서 직접 기본값 읽기. FBPVariableDescription::DefaultValue는 유저가 명시적으로 바꾼 경우만 채워진다.
		FString ValueStr;
		if (CDO)
		{
			if (FProperty* Prop = GeneratedClass->FindPropertyByName(Var.VarName))
			{
				Prop->ExportTextItem_InContainer(ValueStr, CDO, nullptr, CDO, PPF_None);
			}
		}
		if (ValueStr.IsEmpty())
		{
			ValueStr = Var.DefaultValue;
		}

		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("type"), TypeStr);
		Entry->SetStringField(TEXT("value"), ValueStr);
		Root->SetObjectField(Var.VarName.ToString(), Entry.ToSharedRef());
	}
	return Root;
}

TSharedPtr<FJsonObject> FDwBlueprintSnapshotExporter::BuildInterfacesJson(UBlueprint* Blueprint)
{
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FBPInterfaceDescription& Desc : Blueprint->ImplementedInterfaces)
	{
		if (Desc.Interface)
		{
			Arr.Add(MakeShared<FJsonValueString>(Desc.Interface->GetPathName()));
		}
	}

	if (Arr.Num() == 0)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("implemented"), Arr);
	return Root;
}

FString FDwBlueprintSnapshotExporter::SerializeJson(TSharedRef<FJsonObject> RootObject)
{
	SortJsonObjectRecursive(RootObject);

	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(RootObject, Writer);
	return Out;
}

FString FDwBlueprintSnapshotExporter::ResolveSnapshotPath(const FString& PackageName)
{
	if (PackageName.IsEmpty())
	{
		return FString();
	}

	const UDwBlueprintSnapshotSettings* Settings = GetDefault<UDwBlueprintSnapshotSettings>();
	if (!Settings)
	{
		return FString();
	}

	FString RootDir = Settings->OutputDirectory.Path;
	if (RootDir.IsEmpty())
	{
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DwBlueprintSnapshot"));
		if (!Plugin.IsValid())
		{
			return FString();
		}
		RootDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Snapshots"));
	}
	else if (FPaths::IsRelative(RootDir))
	{
		RootDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), RootDir);
	}

	// /Game/UI/Widget/WBP_Ability -> Game/UI/Widget/WBP_Ability{Ext}
	FString PackagePath = PackageName;
	if (PackagePath.StartsWith(TEXT("/")))
	{
		PackagePath.RemoveAt(0);
	}

	return FPaths::Combine(RootDir, PackagePath) + Settings->FileExtension;
}
