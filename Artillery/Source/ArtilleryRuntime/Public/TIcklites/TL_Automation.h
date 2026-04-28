#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FGunKey.h"
#include "ArtilleryDispatch.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Autogun Machinery: Base, Wrapper, and Examples
// Cadenced Ticklite skeleton, and cadenced autogun are your examples, the machinery is in base and wrapper.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FAutoGunCommunication
{
	bool AmIOn = false;
	bool AmIReady = false;
	bool ShouldTickAtAll = false;
	FSkeletonKey MyKey;
};

//An auto gun is a ticklite that _somehow_ figures out when to trigger an Artillery gun.
//This could be timed, this could be conditional, this could be all sorts of things.
//Generally, if it uses complex query logic, there's a good chance that most of that should live in the ticklite
//as artillery guns fire on the gamethread, but I actively encourage you to prototype it in the prefire THEN move it to the ticklite.
//The gun's prefire should probably handle ammon requirements and stuff like that.
template<typename AutoGunTicklite>
class AutoWrapperForGun : public FAutoGunCommunication
{
	using AutoGunTicklitePtr = AutoGunTicklite*;
	
public:
	AutoWrapperForGun(UArtilleryDispatch* Dispatch, AutoGunTicklitePtr MyGunTriggerHandler, uint16 CadenceInTicks,
		bool IsThisAnAITurret, bool ProcessAIRequests): Dispatch(Dispatch), CadenceAndQuery(MyGunTriggerHandler)
	{
		if(IsThisAnAITurret == false || ProcessAIRequests == true)
		{
			//set request router.
			CadenceAndQuery->Core.SetRouterAndDispatch(Dispatch, Dispatch->RequestRouter);
			//lock in the cadence.
			CadenceAndQuery->Core.SetCadence(CadenceInTicks);
		}
		
		Dispatch->RequestAddTicklite(CadenceAndQuery, Early);
	};
	UArtilleryDispatch* Dispatch;
	AutoGunTicklitePtr CadenceAndQuery; 
};

//Inherit from this! I think you can just implement the virtual functions and use ShouldTickCadencedComponents
//in Apply and Calculate. Calculate should figure out if the gun should fire, and apply should do whatever's needed
//with the FGunkey, your ArtilleryDispatch pointer, and your Request Router to actually fire that gun.
//The gun's prefire should be what checks things like ammunition, but targeting should likely be handled in your implementation
//of the cadenced ticklite inheritor. This creates the cleanest separation of concerns, and lets you reuse both parts
//for multiple actual weapons implementations. Good luck, and use in good health.
///////////////////////////////////////////////////////////////////////////////////////////
//Params:
// TicksToApply is the number of times we want the cadence to trigger. So TicksToApply * My Cadence = total duration
// TicksRemaining is our decrementing counter.
// MyCadence is the number of ticks we allow to pass before we tick anything gated with if(ShouldTickCadencedComponents())
/////
//Closing note:
//A lot of these are going to run forever. That's why the TICKLITE_CheckForExpiration is totally defaulted. So, to make
//an infinite duration ticklite, just set TicksRemaining = MyCadence each time if(ShouldTickCadencedComponents())
class CadencedTicklite : public UArtilleryDispatch::TL_ThreadedImpl
{
public:
	uint32_t TicksToApply;
	uint32_t TicksRemaining; //TODO: make this rollback correctly.
	FGunKey MyGunToFire;
	//You can still have a DIFFERENT dispatch for actually RUNNING your ticklite
	//And MOST ticklites WILL! Because you'll need to hit your executing thread. This is why we don't hide the ADispatch
	//member of TL_ThreadedImpl! Please be aware of this! --J
	UArtilleryDispatch* ArtilleryDispatch; 
	TSharedPtr<F_INeedA> RequestRouter;
	uint8 MyCadence;
	
	CadencedTicklite(uint32_t MyTicksToApply, const FGunKey& AutomateThisGun, uint32_t MyCadence)
	: TicksToApply(MyTicksToApply),
	TicksRemaining(MyTicksToApply),
	MyGunToFire(AutomateThisGun),
	ArtilleryDispatch(nullptr),
	MyCadence(MyCadence)
	{
	}

	virtual void TICKLITE_StateReset() = 0;
	virtual void TICKLITE_Calculate() = 0;
	virtual void TICKLITE_Apply() = 0;
	virtual void TICKLITE_CoreReset() = 0;
	virtual bool TICKLITE_CheckForExpiration() = 0;
	virtual void TICKLITE_OnExpiration() = 0;

	//this might need to be expanded a little so that it's actually useful. prolly gonna wanna add
	//a lil function for telling if this is a Good Tick To Do Shit.
	void SetRouterAndDispatch(UArtilleryDispatch* ArtyDispatch, TSharedPtr<F_INeedA> ARequestRouter)
	{
		ArtilleryDispatch=ArtyDispatch;
		RequestRouter=ARequestRouter;
	};
	
	void SetCadence(uint8 ACadence)
	{
		MyCadence=ACadence;
		TicksToApply = TicksToApply * ACadence;
		TicksRemaining = TicksRemaining * ACadence;
	}
	
	//Use this to wrap Apply and Calculate components that should only run on your cadence.
	//You might want to run some stuff every tick, and some stuff only on the cadence.
	virtual bool ShouldTickCadencedComponents()
	{
		return TicksRemaining % MyCadence == 0;
	}
};

typedef Ticklites::Ticklite<CadencedTicklite> TL_CadencedTicklite;
template<uint8 cadence>
class CadencedAutoGun : public AutoWrapperForGun<TL_CadencedTicklite>
{
	//TSharedPtr<> is enforced.
	CadencedAutoGun(UArtilleryDispatch* Dispatch, TL_CadencedTicklite* GunTriggerTicklite)
	: AutoWrapperForGun(Dispatch, GunTriggerTicklite, cadence, false, false)
	{
	}
};

/* Last bit of info here:
 * Here's a possible alternative for the above. I personally don't like it very much, as it's quite hard to parse what's happening,
 * and I'm not totally sure the template substitution is actually valid in all cases, but it does provide type enforcement without using the
 * somewhat difficult to parse concepts and constraints provided by 17 and 20. Personally, until there's a slightly more consistent
 * interaction between concepts, constraints, and UE's odd template implementations, I'm steering clear for our codebase. I could
 * be talked around though! --J
typedef Ticklites::Ticklite<CadencedTicklite> TL_CadencedTicklite;
template<uint8 cadence, typename YourTickliteInheritingFromCadencedTicklite>
class CadencedAutoGun : public AutoWrapperForGun<Ticklites::Ticklite<YourTickliteInheritingFromCadencedTicklite>>
{
	//TSharedPtr<> is enforced.
	CadencedAutoGun(UArtilleryDispatch* Dispatch, TSharedPtr<Ticklites::Ticklite<YourTickliteInheritingFromCadencedTicklite>> GunTriggerTicklite):
	AutoWrapperForGun<Ticklites::Ticklite<YourTickliteInheritingFromCadencedTicklite>>
	(Dispatch, GunTriggerTicklite, cadence, false, false)
	{
	};
};
 * 
 */

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HARVESTER Skeleton for Slow Determinism
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// harvesters are shims that combined with the concept of a "Wide Cadence" allow you to wrap an entire UE system instance,
// like a set of state trees, and have them run in a deterministic way on the following four conditions:
// They generate only internal state or artillery state.
// They perform their reads and THEN their writes.
// They do not, on a _per execution basis_ need their writes to be resolved before their reads for _the same execution_.
// They can complete one round of execution during a certain knowable-if-large number of ticks.
// These requirements can be relaxed with additional work, but they are the basis assumptions.
// see (see: https://github.com/JKurzer/sd/blob/main/README.md )
template<typename HarvesterTicklite>
class FHarvesterWrapper
{
	FSkeletonKey MyKey;
	Ticklites::Ticklite<HarvesterTicklite> Harvester;
	F_INeedA* MyRequestRouter =			   nullptr; //This should almost never be the artillery dispatch's RequestRouter.
};
