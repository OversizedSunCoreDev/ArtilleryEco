#pragma once

#include "Engine/DataTable.h"

#include "FProjectileDefinitionRow.generated.h"
class UNiagaraDataChannelAsset;
class UStaticMesh;

USTRUCT(BlueprintType)
struct FProjectileDefinitionRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ProjectileDefinition)
	FString ProjectileDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ProjectileDefinition)
	TObjectPtr<UStaticMesh> ProjectileMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ProjectileDefinition)
	TObjectPtr<UNiagaraDataChannelAsset> ParticleEffectDataChannel;
};
