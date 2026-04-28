#pragma once

#include <atomic>
#include <memory>
#include <utility>
#include "NativeGameplayTags.h"
#include "SkeletonTypes.h"

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "Engine/DataTable.h"
#include "Containers/CircularBuffer.h"
#include "ConservedTagContainer.h"




typedef TSharedPtr<FTagStateRepresentation> FTagsPtr;
using FConservedTags = TSharedPtr<FConservedTagContainer> ;
THIRD_PARTY_INCLUDES_START
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END


class ARTILLERYRUNTIME_API AtomicTagArray
{
	friend class UArtilleryDispatch;
	
public:
	AtomicTagArray(const TagsSeen& SeenT) : SeenT(SeenT)
	{
		FastEntities = MakeShareable(new Entities());
	}

	AtomicTagArray();
	bool Add(FSkeletonKey Top, FGameplayTag Bot);
	//lifecycle management for the FConservedTags pointers is out of scope for this class
	//and is instead managed in the client and arbiter, ArtilleryDispatch. I cannot stress this _enough_. Do not try to "fix" this.
	static uint32_t KeyToHash(FSkeletonKey Top);
	bool Find(FSkeletonKey Top, FGameplayTag Bot);
	bool Remove(FSkeletonKey Top, FGameplayTag Bot);
	bool SkeletonKeyExists(FSkeletonKey Top);
	FConservedTags GetReference(FSkeletonKey Top);
	void Init();
	bool Empty();
	
protected:
	//it is STRONGLY advised that you NEVER call EITHER OF THESE directly.
	//please instead use RegisterGameplayTags or DeregisterGameplayTags
	FConservedTags NewTagContainer(FSkeletonKey Top);
	bool Erase(FSkeletonKey Top);
	
private:
	bool AddImpl(uint32_t Key, FGameplayTag Bot);

	unsigned short Counter = 1; //magicify 0.
	TagsSeen SeenT;
	TagsByCode MasterDecoderRing;
	TSharedPtr<Entities> FastEntities;
	constexpr static uint8 STARTED = 1;
	constexpr static uint8 FINISHED = 2;
};
