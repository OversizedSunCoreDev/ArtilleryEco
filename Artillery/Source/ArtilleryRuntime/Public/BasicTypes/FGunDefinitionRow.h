#pragma once


#include "Engine/DataTable.h"
#include "FMasks.h"
#include "FGunDefinitionRow.generated.h"

USTRUCT(BlueprintType)
struct FGunDefinitionRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString GunDefinitionId;
	
	//these could BOTH be true. enjoy that, I guess. it's not implemented though.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	bool IsCPP = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	bool IsBP = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString LoadableCPP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString LoadableBP; 

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString ProjectileDefinitionID; 

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString PreFireAbility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString PreFireCosmeticAbility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString FireAbility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString FireCosmeticAbility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString PostFireAbility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString PostFireCosmeticAbility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FString FailureCosmeticAbility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	int32 BaseDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	int32 BaseRange = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	int32 BaseRateOfFire = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	int32 BaseRecoil = 0;
	
	//Unsure at this point in implementation if this value will always be respected.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	E_ArtilleryIntents IntendedRegistrationPattern = E_ArtilleryIntents::MenuIndex;
};
