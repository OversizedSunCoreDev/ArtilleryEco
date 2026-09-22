#pragma once
#include "FArtilleryGun.h"
#include "SkeletonTypes.h"
#include "Templates/TypeHash.h"
THIRD_PARTY_INCLUDES_START
#include "seq/ordered_map.hpp"
#include "seq/concurrent_map.hpp"
#include "seq/radix_hash_map.hpp"
#include "seq/SeqU64Prefix.hpp"
#include "seq/flat_map.hpp"
#include "Structures/ApproximateMembership/FLargeGate.h"
THIRD_PARTY_INCLUDES_END

#include "InventoryEssentialTypes.h"
#include "InventoryDispatch.h"
#include "FQuestGun.generated.h"


USTRUCT()
struct FQuestTriggerGun : public FSimpleTriggerGun
{
		
	GENERATED_BODY()
	FTriggerInstance MyTriggerInstance;
public:
	virtual void PostFireGun(
	FArtilleryStates OutcomeStates,
	int DallyFramesToOmit,
	bool RerunDueToReconcile) override
	{
		if (PostCheck())
		{
			//TODO: ADD AUTOFIRE QUEST ADVANCE which will CLOSE THIS TRIGGER
		}
			
	};

	virtual bool PostCheck()
	{
		return true;
	}
	friend uint32 GetTypeHash(const FQuestTriggerGun& Arg)
	{
		return Arg.MyGunKey.GunInstanceID; //these are valid hashes, interestingly.
	}
		
	//unlike almost all types, for us, equality is the same as comparing hashes, because
	//our hash is our key and it's precomputed with a known min\non-collisive trick _somewhere_
	//in this case, the upper 32 bits are deterministic information necessary for not hating yourself forever
	//and the lower 32 bits are an instance hash. 
	bool operator==(const FQuestTriggerGun& other) const {
		return MyGunKey == other.MyGunKey; 
	}
};
namespace std {
	template <>
	struct hash<FQuestTriggerGun> {
		std::size_t operator()(const FQuestTriggerGun& u) const noexcept {
			return u.MyGunKey.GunInstanceID;
		}
	};
}