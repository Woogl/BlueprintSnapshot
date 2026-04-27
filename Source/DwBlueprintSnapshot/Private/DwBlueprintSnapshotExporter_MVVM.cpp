// Copyright Woogle. All Rights Reserved.

//
// MVVM ViewModel 컨텍스트 + 바인딩 추출.
// 엔진 업그레이드 주의: MVVM 플러그인 내부 헤더(MVVMBlueprintView 등)를 직접 include한다.
// UE 5.8+에서 MVVM API가 바뀌면 이 파일만 손보면 되도록 격리해 두었다.
//

#include "DwBlueprintSnapshotExporter.h"
#include "WidgetBlueprint.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMBlueprintFunctionReference.h"
#include "MVVMBlueprintPin.h"
#include "MVVMPropertyPath.h"
#include "Types/MVVMBindingMode.h"
#include "UObject/Class.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	FString BuildPinBindingString(const FMVVMBlueprintPin& Pin, const UClass* SelfContext)
	{
		FString Value;
		if (Pin.UsedPathAsValue())
		{
			Value = Pin.GetPath().GetPropertyPath(SelfContext);
		}
		else
		{
			Value = Pin.GetValueAsString(SelfContext);
		}
		if (Value.IsEmpty())
		{
			Value = TEXT("(default)");
		}
		if (Pin.GetStatus() == EMVVMBlueprintPinStatus::Orphaned)
		{
			Value += TEXT(" [orphaned]");
		}
		return Value;
	}

	FString BuildConversionFunctionName(const UMVVMBlueprintViewConversionFunction& Conversion, const UBlueprint* Blueprint)
	{
		const FMVVMBlueprintFunctionReference FuncRef = Conversion.GetConversionFunction();

		FString FunctionName = FuncRef.GetName().ToString();
		if (FunctionName.IsEmpty() && Blueprint)
		{
			if (const UFunction* Func = FuncRef.GetFunction(Blueprint))
			{
				FunctionName = Func->GetName();
			}
		}
		if (FunctionName.IsEmpty())
		{
			FunctionName = FuncRef.ToString();
		}
		if (FunctionName.IsEmpty())
		{
			FunctionName = TEXT("(unknown)");
		}
		return FunctionName;
	}

	TSharedPtr<FJsonObject> BuildConversionJson(const UMVVMBlueprintViewConversionFunction* Conversion, const UBlueprint* Blueprint)
	{
		if (!Conversion || !Blueprint)
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("function"), BuildConversionFunctionName(*Conversion, Blueprint));

		const UClass* SelfContext = Blueprint->GeneratedClass;

		TSharedPtr<FJsonObject> ArgsJson = MakeShared<FJsonObject>();
		for (const FMVVMBlueprintPin& Pin : Conversion->GetPins())
		{
			const TArrayView<const FName> Names = Pin.GetId().GetNames();
			if (Names.Num() == 0)
			{
				continue;
			}

			TArray<FString> NameStrs;
			NameStrs.Reserve(Names.Num());
			for (const FName& N : Names)
			{
				NameStrs.Add(N.ToString());
			}
			const FString Key = FString::Join(NameStrs, TEXT("."));

			ArgsJson->SetStringField(Key, BuildPinBindingString(Pin, SelfContext));
		}

		if (ArgsJson->Values.Num() > 0)
		{
			Json->SetObjectField(TEXT("arguments"), ArgsJson.ToSharedRef());
		}

		return Json;
	}
}

TSharedPtr<FJsonObject> FDwBlueprintSnapshotExporter::BuildMvvmJson(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint)
	{
		return nullptr;
	}

	UMVVMWidgetBlueprintExtension_View* Extension = UWidgetBlueprintExtension::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint);
	if (!Extension)
	{
		return nullptr;
	}

	const UMVVMBlueprintView* View = Extension->GetBlueprintView();
	if (!View)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> ViewModelsMap = MakeShared<FJsonObject>();
	for (const FMVVMBlueprintViewModelContext& Ctx : View->GetViewModels())
	{
		TSharedPtr<FJsonObject> VmJson = MakeShared<FJsonObject>();
		VmJson->SetStringField(TEXT("class"), Ctx.GetViewModelClass() ? Ctx.GetViewModelClass()->GetPathName() : FString());
		VmJson->SetStringField(TEXT("creationType"), StaticEnum<EMVVMBlueprintViewModelContextCreationType>()->GetNameStringByValue(static_cast<int64>(Ctx.CreationType)));
		if (!Ctx.GlobalViewModelIdentifier.IsNone())
		{
			VmJson->SetStringField(TEXT("globalIdentifier"), Ctx.GlobalViewModelIdentifier.ToString());
		}
		if (!Ctx.ViewModelPropertyPath.IsEmpty())
		{
			VmJson->SetStringField(TEXT("propertyPath"), Ctx.ViewModelPropertyPath);
		}
		VmJson->SetStringField(TEXT("contextId"), Ctx.GetViewModelId().ToString(EGuidFormats::DigitsWithHyphens));
		VmJson->SetBoolField(TEXT("optional"), Ctx.bOptional);
		VmJson->SetBoolField(TEXT("createSetter"), Ctx.bCreateSetterFunction);
		VmJson->SetBoolField(TEXT("createGetter"), Ctx.bCreateGetterFunction);

		ViewModelsMap->SetObjectField(Ctx.GetViewModelName().ToString(), VmJson.ToSharedRef());
	}

	const UClass* SelfContext = WidgetBlueprint->GeneratedClass;
	struct FBindingEntry
	{
		FString SortKey;
		TSharedPtr<FJsonObject> Json;
	};
	TArray<FBindingEntry> Entries;
	for (const FMVVMBlueprintViewBinding& Binding : View->GetBindings())
	{
		if (!Binding.bEnabled || !Binding.bCompile)
		{
			continue;
		}

		const FString Source = Binding.SourcePath.GetPropertyPath(SelfContext);
		const FString Destination = Binding.DestinationPath.GetPropertyPath(SelfContext);

		TSharedPtr<FJsonObject> BJson = MakeShared<FJsonObject>();
		BJson->SetStringField(TEXT("source"), Source);
		BJson->SetStringField(TEXT("destination"), Destination);
		BJson->SetStringField(TEXT("bindingType"), StaticEnum<EMVVMBindingMode>()->GetNameStringByValue(static_cast<int64>(Binding.BindingType)));

		if (UMVVMBlueprintViewConversionFunction* SrcToDst = Binding.Conversion.GetConversionFunction(true))
		{
			if (TSharedPtr<FJsonObject> ConvJson = BuildConversionJson(SrcToDst, WidgetBlueprint))
			{
				BJson->SetObjectField(TEXT("conversionSourceToDestination"), ConvJson.ToSharedRef());
			}
		}
		if (UMVVMBlueprintViewConversionFunction* DstToSrc = Binding.Conversion.GetConversionFunction(false))
		{
			if (TSharedPtr<FJsonObject> ConvJson = BuildConversionJson(DstToSrc, WidgetBlueprint))
			{
				BJson->SetObjectField(TEXT("conversionDestinationToSource"), ConvJson.ToSharedRef());
			}
		}

		Entries.Add({ FString::Printf(TEXT("%s|%s|%d"), *Source, *Destination, static_cast<int32>(Binding.BindingType)), BJson });
	}
	Entries.Sort([](const FBindingEntry& A, const FBindingEntry& B) { return A.SortKey < B.SortKey; });

	if (ViewModelsMap->Values.Num() == 0 && Entries.Num() == 0)
	{
		return nullptr;
	}

	if (ViewModelsMap->Values.Num() > 0)
	{
		Root->SetObjectField(TEXT("viewModels"), ViewModelsMap.ToSharedRef());
	}

	if (Entries.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> BindingsArr;
		BindingsArr.Reserve(Entries.Num());
		for (const FBindingEntry& Entry : Entries)
		{
			BindingsArr.Add(MakeShared<FJsonValueObject>(Entry.Json));
		}
		Root->SetArrayField(TEXT("bindings"), BindingsArr);
	}

	return Root;
}
