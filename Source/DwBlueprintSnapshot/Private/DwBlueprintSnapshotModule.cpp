// Copyright Woogle. All Rights Reserved.

#include "DwBlueprintSnapshotModule.h"
#include "DwBlueprintSnapshotSettings.h"
#include "DwBlueprintSnapshotExporter.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EditorUtilityBlueprint.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/ObjectSaveContext.h"
#include "Modules/ModuleManager.h"
#include "Misc/CommandLine.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "HAL/PlatformFileManager.h"

IMPLEMENT_MODULE(FDwBlueprintSnapshotModule, DwBlueprintSnapshot)

DEFINE_LOG_CATEGORY(LogDwBPSnapshot);

void FDwBlueprintSnapshotModule::StartupModule()
{
	PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(this, &FDwBlueprintSnapshotModule::HandlePackageSaved);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FDwBlueprintSnapshotModule::HandleTick), 0.0f);

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddRaw(this, &FDwBlueprintSnapshotModule::HandleAssetRemoved);
}

void FDwBlueprintSnapshotModule::ShutdownModule()
{
	if (PackageSavedHandle.IsValid())
	{
		UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
		PackageSavedHandle.Reset();
	}
	if (AssetRemovedHandle.IsValid())
	{
		if (FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
		{
			Module->Get().OnAssetRemoved().Remove(AssetRemovedHandle);
		}
		AssetRemovedHandle.Reset();
	}
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	PendingQueue.Empty();
	PendingPaths.Empty();
}

bool FDwBlueprintSnapshotModule::ShouldProcessContext(const FObjectPostSaveContext& Context) const
{
	if (Context.IsProceduralSave() || Context.IsCooking() || Context.IsFromAutoSave())
	{
		return false;
	}
	if (IsRunningCommandlet())
	{
		return false;
	}
	return true;
}

bool FDwBlueprintSnapshotModule::ShouldProcessBlueprint(UBlueprint* Blueprint) const
{
	if (!Blueprint)
	{
		return false;
	}

	UPackage* Package = Blueprint->GetOutermost();
	if (Package && Package->HasAnyPackageFlags(PKG_PlayInEditor | PKG_ForDiffing | PKG_Cooked))
	{
		return false;
	}

	switch (Blueprint->BlueprintType)
	{
	case BPTYPE_MacroLibrary:
	case BPTYPE_Interface:
	case BPTYPE_FunctionLibrary:
		return false;
	default:
		break;
	}

	if (Blueprint->IsA<UEditorUtilityBlueprint>())
	{
		return false;
	}

	if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->GetDefaultObject(false))
	{
		return false;
	}

	if (Blueprint->Status == BS_Dirty || Blueprint->Status == BS_Error || Blueprint->Status == BS_Unknown)
	{
		UE_LOG(LogDwBPSnapshot, Verbose, TEXT("Skip %s: compile required (status=%d)"), *Blueprint->GetPathName(), static_cast<int32>(Blueprint->Status));
		return false;
	}

	return true;
}

bool FDwBlueprintSnapshotModule::IsPackageNameIncluded(const FString& PackageName) const
{
	const UDwBlueprintSnapshotSettings* Settings = GetDefault<UDwBlueprintSnapshotSettings>();
	if (!Settings)
	{
		return false;
	}

	const FString PackageWithSlash = PackageName + TEXT("/");
	auto MatchesAnyDir = [&PackageWithSlash](const TArray<FDirectoryPath>& Dirs) -> bool
	{
		for (const FDirectoryPath& Dir : Dirs)
		{
			if (Dir.Path.IsEmpty())
			{
				continue;
			}
			const FString Prefix = Dir.Path.EndsWith(TEXT("/")) ? Dir.Path : Dir.Path + TEXT("/");
			if (PackageWithSlash.StartsWith(Prefix))
			{
				return true;
			}
		}
		return false;
	};

	if (Settings->IncludeDirectories.Num() > 0 && !MatchesAnyDir(Settings->IncludeDirectories))
	{
		return false;
	}
	if (MatchesAnyDir(Settings->ExcludeDirectories))
	{
		return false;
	}
	return true;
}

void FDwBlueprintSnapshotModule::HandlePackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext Context)
{
	if (!Package)
	{
		return;
	}

	const UDwBlueprintSnapshotSettings* Settings = GetDefault<UDwBlueprintSnapshotSettings>();
	if (!Settings || !Settings->bEnabled)
	{
		return;
	}

	if (!ShouldProcessContext(Context))
	{
		return;
	}

	const FString PackageName = Package->GetName();
	if (!IsPackageNameIncluded(PackageName))
	{
		return;
	}

	TArray<UObject*> AssetsInPackage;
	GetObjectsWithOuter(Package, AssetsInPackage, false);
	for (UObject* Asset : AssetsInPackage)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		if (!Blueprint)
		{
			continue;
		}
		if (!ShouldProcessBlueprint(Blueprint))
		{
			continue;
		}
		EnqueueBlueprint(Blueprint);
	}
}

void FDwBlueprintSnapshotModule::HandleAssetRemoved(const FAssetData& AssetData)
{
	const UDwBlueprintSnapshotSettings* Settings = GetDefault<UDwBlueprintSnapshotSettings>();
	if (!Settings || !Settings->bEnabled)
	{
		return;
	}

	if (IsRunningCommandlet())
	{
		return;
	}

	// Blueprint 계열 자산만 대상. UWidgetBlueprint 등 파생 클래스도 포함.
	if (!AssetData.IsInstanceOf<UBlueprint>())
	{
		return;
	}

	const FString PackageName = AssetData.PackageName.ToString();
	if (!IsPackageNameIncluded(PackageName))
	{
		return;
	}

	const FString SnapshotPath = FDwBlueprintSnapshotExporter::ResolveSnapshotPath(PackageName);
	if (SnapshotPath.IsEmpty())
	{
		return;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*SnapshotPath))
	{
		return;
	}

	if (PlatformFile.IsReadOnly(*SnapshotPath))
	{
		PlatformFile.SetReadOnly(*SnapshotPath, false);
	}

	if (PlatformFile.DeleteFile(*SnapshotPath))
	{
		UE_LOG(LogDwBPSnapshot, Log, TEXT("Snapshot deleted for %s"), *PackageName);
	}
	else
	{
		UE_LOG(LogDwBPSnapshot, Warning, TEXT("Failed to delete snapshot %s"), *SnapshotPath);
	}

	// 삭제된 BP가 큐에 대기 중이면 제거하여 뒤늦은 재기록을 방지.
	PendingPaths.Remove(AssetData.GetObjectPathString());
}

void FDwBlueprintSnapshotModule::EnqueueBlueprint(UBlueprint* Blueprint)
{
	FString Path = Blueprint->GetPathName();
	bool bAlreadyQueued = false;
	PendingPaths.Add(Path, &bAlreadyQueued);
	if (bAlreadyQueued)
	{
		return;
	}
	PendingQueue.Enqueue({ TWeakObjectPtr<UBlueprint>(Blueprint), MoveTemp(Path) });
}

bool FDwBlueprintSnapshotModule::HandleTick(float DeltaTime)
{
	FPendingEntry Entry;
	if (!PendingQueue.Dequeue(Entry))
	{
		return true;
	}

	PendingPaths.Remove(Entry.Path);

	UBlueprint* Blueprint = Entry.Blueprint.Get();
	if (Blueprint && ShouldProcessBlueprint(Blueprint))
	{
		const bool bWritten = FDwBlueprintSnapshotExporter::ExportBlueprint(Blueprint);
		if (bWritten)
		{
			UE_LOG(LogDwBPSnapshot, Log, TEXT("Snapshot written for %s"), *Blueprint->GetPathName());
		}
	}

	return true;
}
