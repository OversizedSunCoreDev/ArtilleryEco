// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ArtilleryDeveleperSettings.generated.h"

/**
 * 
 */
UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "Artillery"))
class UArtilleryDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, config, meta = (RequiredAssetDataTags = "RowStructure=/Script/ArtilleryRuntime.GunDefinitionRow"))
	TSoftObjectPtr<UDataTable> GamePath = TSoftObjectPtr<UDataTable>(FSoftObjectPath("DataTable'/Game/DataTables/GunDefinitions.GunDefinitions'"));
	
	//we don't really recommend using this path for long, but we ship with it because we believe you should be
	//able to run software. I k n o w I'm old fashioned.
	UPROPERTY(EditAnywhere, config, meta = (RequiredAssetDataTags = "RowStructure=/Script/ArtilleryRuntime.GunDefinitionRow"))
	TSoftObjectPtr<UDataTable> EcoPath = TSoftObjectPtr<UDataTable>(FSoftObjectPath("DataTable'/Artillery/DataTables/GunDefinitions.GunDefinitions'"));
	
	// This enables things to call the meta to get a nice list of guns. Probably not always desirable as it might prevent manually entering things when needed = (GetOptions = "ArtilleryRuntime.ArtilleryDeveloperSettings.GetGunNamesForEditorSelection")
	UPROPERTY(EditAnywhere, config)
	bool bUseGetOptionsForGunNames = true;
	
	UFUNCTION()
	static TArray<FName> GetGunNamesForEditorSelection();
	
public:
	UDataTable* LoadMainGunsDataTable() const ;
};