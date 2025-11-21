#pragma once

#include "CoreMinimal.h"
#include "ArtilleryShell.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"
#include "UObject/ScriptMacros.h"
#include "KeyedConcept.h"
#include "Ticklite.h"
#include "ArtilleryDispatch.h"
#include "FArtilleryTicklitesThread.h"




struct FTickECSOnly : public UArtilleryDispatch::TL_ThreadedImpl
{
	
	FSkeletonKey Target;
	FTickECSOnly() : TL_ThreadedImpl()
	{
	}

	//there's no nan check here right now.. that's not in the scope of this code. please don't hand nan forces around.
	FTickECSOnly(FSkeletonKey TargetIn) : TL_ThreadedImpl()
	{
		Target = TargetIn;
	}

	void TICKLITE_StateReset()
	{
	}

	void TICKLITE_Calculate()
	{
	}
	
	virtual void TICKLITE_Apply() {
		if(FBLet GameSimPhysicsObject = this->ADispatch->GetFBLetByObjectKey(Target, this->ADispatch->GetShadowNow()))
		{
			ArtilleryTick(this->GetShadowNow());
		}
	}

	void TICKLITE_CoreReset() {
	}

	bool TICKLITE_CheckForExpiration() {
		if(FBLet GameSimPhysicsObject = this->ADispatch->GetFBLetByObjectKey(Target, this->ADispatch->GetShadowNow()))
		{
			return false;
		}
		return true;
	}

	void TICKLITE_OnExpiration() {
		
	}

	//This Artillery Tick must ONLY use target skeleton key.
	//This creates an entirely safe mutating tick.
	//While there are ways to enforce it at compile time, we don't do so, in large part because
	//there are also ways to make it safe, and this is likely to be only used in a few places,
	//deep in the meat of the beast.
	virtual void ArtilleryTick(uint64_t TicksSoFar) 
	{
		
	}
};

typedef Ticklites::Ticklite<FTickECSOnly> StartTicker;


