// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "ThistleTypes.h"
#include "Blueprint/StateTreeEvaluatorBlueprintBase.h"
#include "ThistleEvaluators.generated.h"

USTRUCT(meta = (Hidden))
struct THISTLERUNTIME_API FThistleMSEvaluator : public FStateTreeEvaluatorBase
{
	GENERATED_BODY()
};

USTRUCT(meta = (DisplayName = "Get Player Key"))
struct THISTLERUNTIME_API FThistleGetPlayerKey : public FThistleMSEvaluator
{
	GENERATED_BODY()

	using FInstanceDataType = F_KOutOnlyInstanceData;
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT(meta = (DisplayName = "Get Related Artillery Key"))
struct THISTLERUNTIME_API FThistleKeyToRelationship : public FThistleMSEvaluator
{
	GENERATED_BODY()

	using FInstanceDataType = F_RelatedKey;
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override {return true;};
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT(meta = (DisplayName = "Zap! Fire a spherecast!"))
struct THISTLERUNTIME_API FThistleSphereCast : public FThistleMSEvaluator
{
	GENERATED_BODY()

	using FInstanceDataType = FThistleSphereCastInstanceData;
	
protected:
	virtual bool Link(FStateTreeLinker& Linker) override {return true;};
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT()
struct FThistleDistanceToPOIInstanceData
{
	GENERATED_BODY()

	// Properties bound to the keys of the AI and the Player.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (BindWidget = "Input"))
	F_TPOIInstanceData Source;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (BindWidget = "Input"))
	F_TPOIInstanceData Target;

	// Calculated distance that will be bound as an output property in the State Tree.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (BindWidget = "Output"))
	float Distance = 0.0f;
};

USTRUCT(meta = (DisplayName = "Calculate Distance To POI"))
struct THISTLERUNTIME_API FThistleDistanceToPOI : public FThistleMSEvaluator
{
	GENERATED_BODY()

	using FInstanceDataType = FThistleDistanceToPOIInstanceData;

protected:
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};