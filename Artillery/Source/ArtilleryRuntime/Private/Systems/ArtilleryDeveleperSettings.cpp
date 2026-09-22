// Fill out your copyright notice in the Description page of Project Settings.


#include "ArtilleryDeveleperSettings.h"

#include "ArtilleryRuntimeModule.h"

TArray<FName> UArtilleryDeveloperSettings::GetGunNamesForEditorSelection()
{
	const UArtilleryDeveloperSettings* CDO = GetDefault<UArtilleryDeveloperSettings>();

	if (!CDO->bUseGetOptionsForGunNames)
	{
		return {};
	}

	if (const UDataTable* DataTable = CDO->LoadMainGunsDataTable())
	{
		return DataTable->GetRowNames();
	}
	
	return {"No Artillery Guns datatable found! Check Artillery Settings! (UArtilleryDeveloperSettings)"};
}

UDataTable* UArtilleryDeveloperSettings::LoadMainGunsDataTable() const 
{
	//@todo a this object is small so this loadsync isn't bad but it will force other loadsynchs to flush 
	// Try the game-specific path before the eco path
	UDataTable* DataTable = GamePath.LoadSynchronous();
	// (this should probably be something easy to understand? log it?)
	if (!DataTable)
	{
		UE_LOG(LogArtillery, Warning, TEXT("UArtilleryDeveloperSettings::LoadMainGunsDataTable failed to load GamePath guns data table path at %s!, falling back to eco table"), *GamePath.ToString());
		DataTable = EcoPath.LoadSynchronous();
	}
	
	if (DataTable)
	{
		return DataTable;
	}
	
	UE_LOG(LogArtillery, Log, TEXT("UArtilleryDeveloperSettings::LoadMainGunsDataTable failed to load a data table!"));
	return nullptr;
}
