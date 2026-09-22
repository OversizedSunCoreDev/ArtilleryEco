#pragma once


#include "Engine/DataTable.h"
#include "FMasks.h"
#include "FGunDefinitionRow.generated.h"

class UArtilleryGunBlueprint;

USTRUCT(BlueprintType)
struct FGunDefinitionRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FName GunDefinitionId;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition,  meta = (MetaStruct="/Script/ArtilleryRuntime.ArtilleryGun"))
	TSoftObjectPtr<UScriptStruct> LoadableCPP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	TSoftClassPtr<UArtilleryGunBlueprint> LoadableBP; 

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GunDefinition)
	FName ProjectileDefinitionID; 

	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<UAGunBitBP> PreFireAbility;

	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<UAGunBitBP> PreFireCosmeticAbility;

	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<UAGunBitBP>  FireAbility;

	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<UAGunBitBP>  FireCosmeticAbility;

	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<UAGunBitBP>  PostFireAbility;

	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<UAGunBitBP> PostFireCosmeticAbility;

	UPROPERTY(EditDefaultsOnly)
	TSoftClassPtr<UAGunBitBP>  FailureCosmeticAbility;

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
	
	UPROPERTY(BlueprintReadWrite)
	TMap<E_AttribKey, float> Attributes;
	UPROPERTY(BlueprintReadWrite)
	TMap<E_VectorAttrib, FVector> VectorAttributes;
	
};
