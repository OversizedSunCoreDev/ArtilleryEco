#pragma once

#include "Ticklite.h"
#include "ArtilleryDispatch.h"
#include "FArtilleryTicklitesThread.h"

//this is broken out from the other timed event ticklites because we use it so much
//that the reduction in boilerplate is well worth it.
//this ONLY takes a direction and should thus only be used for stuff that happens within 10-20 ticks.
//more than that, and you'll want an expire callback or comparable.
//why isn't this more "feature rich?" YAGNI. 
class FTDelayedForce : public UArtilleryDispatch::TL_ThreadedImpl
{
private:
	FSkeletonKey Target;
	FVector Bop;
	int32 TicksRemaining;

public:
	FTDelayedForce() : TL_ThreadedImpl(), Bop()
	{
		TicksRemaining = 120;
	}

	//there's no nan check here right now.. that's not in the scope of this code. please don't hand nan forces around.
	FTDelayedForce(FSkeletonKey TargetIn, FVector ForceToApply, int32 TickCountStart) : TL_ThreadedImpl()
	{
		Target = TargetIn;
		TicksRemaining = TickCountStart;
		Bop = ForceToApply;
	}

	void TICKLITE_StateReset()
	{
	}

	void TICKLITE_Calculate()
	{
	}
	
	void TICKLITE_Apply() {
		TicksRemaining--;
	}

	void TICKLITE_CoreReset() {
	}

	bool TICKLITE_CheckForExpiration() {
		return TicksRemaining <= 0;
	}

	void TICKLITE_OnExpiration() {
		FBLet GameSimPhysicsObject = this->ADispatch->GetFBLetByObjectKey(Target, this->ADispatch->GetShadowNow());
		if(GameSimPhysicsObject)
		{
			FBarragePrimitive::ApplyForce(Bop, GameSimPhysicsObject);
		}
	}
};

typedef Ticklites::Ticklite<FTDelayedForce> TL_DelayedForce;
typedef Ticklites::Ticklite<FTDelayedForce> TL_Bop;

