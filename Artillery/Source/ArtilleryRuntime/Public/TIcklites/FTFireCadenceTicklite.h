#pragma once

#include "TL_Automation.h"

class FTFireCadenceTicklite : public CadencedTicklite
{
public:
	FTFireCadenceTicklite(uint32_t MyTicksToApply, const FGunKey& AutomateThisGun, uint32_t MyCadence = 1)
	: CadencedTicklite(MyTicksToApply, AutomateThisGun, MyCadence)
	{
		
	}

	FTFireCadenceTicklite() : FTFireCadenceTicklite(0, Default, 1)
	{
	}

	void TICKLITE_StateReset()
	{
	}

	void TICKLITE_Calculate()
	{
		ShouldFire = ShouldTickCadencedComponents();
	}

	void TICKLITE_Apply()
	{
		if (ShouldFire)
		{
			ArtilleryDispatch->RequestRouter->GunFired(MyGunToFire, ArtilleryDispatch->GetShadowNow());
		}
		
		// if (TicksRemaining <= 0)
		// {
		// 	SetCadence(1);
		// }
		--TicksRemaining;
	}

	void TICKLITE_CoreReset()
	{
	}

	bool TICKLITE_CheckForExpiration()
	{
		// TODO - stop firing when the player is dead
		return false;
	}

	void TICKLITE_OnExpiration()
	{
	}

private:
	bool ShouldFire = false;
	
	static const inline FGunKey Default = FGunKey("DefaultAutoDeflect");
};
using FTAutoDeflectCadenceTicklite  = FTFireCadenceTicklite;
using TL_AutoDeflectCadenceTicklite = Ticklites::Ticklite<FTFireCadenceTicklite>;
using TL_AutoAutoCannonCadenceTicklite = Ticklites::Ticklite<FTFireCadenceTicklite>;

