// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "Engine/DataTable.h"
#include "Containers/CircularBuffer.h"
#include "BristleconeCommonTypes.h"
#include "ArtilleryCommonTypes.h"

#include "ArtilleryShell.generated.h"

/**
 Artillery provides an opaque storage optimized containerization for the bristlecone packed input, 
 effectively hiding the complexities from the rest of the system. We will need to refactor this to an abstract 
 class and specialize it for inputs, as M+KB starts to matter. That machinery is best suited to living elsewhere,
 as we work towards supporting remap through reuse of EnhancedInput. While we can't use their event loop, 
 we CAN use everything else in EnhancedInput. Let's be careful not to rewrite it. (oops) We DO need a concept of an
 input having run at least once, though, which is not very idiomatic for Einp.
 */

USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FArtilleryShell
{
	GENERATED_BODY()
public:
	//by this point, this is a record of the functions that should be triggered by the keymapping.
	//Mapping from keys to gameplay outcomes (intents) happens outside of Artillery, in cabling.
	//this means that you can reliably re-execute remote input without needing their control mappings, and is fairly essential
	//to maintaining sanity. 
	TheCone::PacketElement MyInputActions; 
	//Packed as:
	//MSB[sticks][buttons][Events]LSB

	void FlagRun()
	{
		RunAtLeastOnce = true;
	}
	BristleTime SentAt;
	ArtilleryTime ReachedArtilleryAt;
	bool RunAtLeastOnce = false; // if this is set, all artillery abilities spawned by running this input will be treated as having run at least once, and will not spawn cosmetic cues. Some animations may still play.
	
	//unpack as floats using the bristlecone packer logic. this is cross-machine deterministic.
	float GetStickLeftX();
	int32_t GetStickLeftXAsACSN();
	float GetStickLeftY();
	int32_t GetStickLeftYAsACSN();
	float GetStickRightX();
	int32_t GetStickRightXAsACSN();
	float GetStickRightY();
	int32_t GetStickRightYAsACSN();

	bool GetInputAction(uint8 inputActionIndex);
	uint32 GetButtonsAndEventsFlat();
private:
	
	//TODO ADD METHODS FOR GET STICKS, GET BUTTONS, GET EVENTS.
	//DO NOT LET THE BITBASHING OUT OF THE BOX.
};
