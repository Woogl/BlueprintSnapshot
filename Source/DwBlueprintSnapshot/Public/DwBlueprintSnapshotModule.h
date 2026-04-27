// Copyright Woogle. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"
#include "UObject/WeakObjectPtr.h"

DWBLUEPRINTSNAPSHOT_API DECLARE_LOG_CATEGORY_EXTERN(LogDwBPSnapshot, Log, All);

class UBlueprint;
class UPackage;
class FObjectPostSaveContext;
struct FAssetData;

/**
 * BP 저장 시 스냅샷을 JSON으로 추출하는 에디터 모듈.
 * PostEngineInit 단계에서 PackageSavedWithContextEvent 를 구독한다.
 * 저장은 큐잉되고 Ticker 가 프레임당 하나씩 처리해 에디터 스파이크를 방지한다.
 */
class FDwBlueprintSnapshotModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void HandlePackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext Context);
	void HandleAssetRemoved(const FAssetData& AssetData);

	bool ShouldProcessContext(const FObjectPostSaveContext& Context) const;
	bool ShouldProcessBlueprint(UBlueprint* Blueprint) const;
	bool IsPackageNameIncluded(const FString& PackageName) const;

	void EnqueueBlueprint(UBlueprint* Blueprint);
	bool HandleTick(float DeltaTime);

	struct FPendingEntry
	{
		TWeakObjectPtr<UBlueprint> Blueprint;
		FString Path;
	};

	FDelegateHandle PackageSavedHandle;
	FDelegateHandle AssetRemovedHandle;
	FTSTicker::FDelegateHandle TickerHandle;

	TQueue<FPendingEntry, EQueueMode::Spsc> PendingQueue;
	TSet<FString> PendingPaths;
};
