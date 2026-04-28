// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "Engine/DataTable.h"
#include "AttributeSet.h"
#include "Containers/CircularBuffer.h"

#include "ConservedAttribute.generated.h"

/**
 * Conserved attributes record their last 128 changes.
 * Currently, this is for debug purposes, but we can use it with some additional features to provide a really expressive
 * model for rollback at a SUPER granular level if needed. 
 */
//TODO: do we need to break the GAS dependency? It's forcing a lot of unneeded stuff.
USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FConservedAttributeData : public FGameplayAttributeData
{
	GENERATED_BODY()
	TCircularBuffer<double> CurrentHistory = TCircularBuffer<double>(128);
	TCircularBuffer<double> RemoteHistory = TCircularBuffer<double>(128);
	TCircularBuffer<double> BaseHistory = TCircularBuffer<double>(128);

	virtual void SetCurrentValue(float NewValue) override {
		SetCurrentValue(static_cast<double>(NewValue));
	};

	virtual void SetCurrentValue(double NewValue) {
		CurrentHistory[CurrentHistory.GetNextIndex(CurrentHead)] = CurrentValue;
		CurrentValue = NewValue;
		++CurrentHead;
	};

	virtual void AddToCurrentValue(double AddValue)
	{
		SetCurrentValue(CurrentValue + AddValue);
	}

	virtual void SetRemoteValue(float NewValue) {
		SetRemoteValue(static_cast<double>(NewValue));
	};
	
	virtual void SetRemoteValue(double NewValue) {
		RemoteHistory[RemoteHistory.GetNextIndex(RemoteHead)] = NewValue;
		++RemoteHead;
	};
	
	virtual void SetBaseValue(float NewValue) override {
		SetBaseValue(static_cast<double>(NewValue));
	};

	virtual void SetBaseValue(double NewValue) {
		BaseHistory[BaseHistory.GetNextIndex(BaseHead)] = BaseValue;
		BaseValue = NewValue;
		++BaseHead;
	};

	double GetPriorValue()
	{
		return CurrentHistory[CurrentHistory.GetNextIndex(CurrentHead-2)]; // ugh
	}
	
	double operator*(FConservedAttributeData const& rhs) 
	{ 
		return CurrentValue * rhs.CurrentValue; // this is a double op.
	};
	
	double operator*(int const& rhs) 
	{ 
		return CurrentValue * rhs; // this is a double op.
	}
	
	double operator*(uint64 const& rhs) 
	{ 
		return CurrentValue * rhs; // this is a double op.
	}
	
protected:
	uint64_t BaseHead = 0;
	uint64_t CurrentHead = 0;
	uint64_t RemoteHead = 0;
};

