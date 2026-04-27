// Copyright Woogle. All Rights Reserved.

#include "DwBlueprintSnapshotSettings.h"

UDwBlueprintSnapshotSettings::UDwBlueprintSnapshotSettings()
{
	CategoryName = TEXT("Dw");
	SectionName = TEXT("DwBlueprintSnapshot");
	OutputDirectory.Path = TEXT("Plugins/DwBlueprintSnapshot/Snapshots");
}
