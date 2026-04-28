#pragma once

#include "Ticklite.h"
#include "ArtilleryDispatch.h"
#include "FArtilleryTicklitesThread.h"

//A ticklite's impl component(s) must provide:
//TICKLITE_StateReset on the memory block aspect
//TICKLITE_Calculate on the impl aspect
//TICKLITE_Apply on the impl aspect, consuming the memory block aspect's state
//TICKLITE_CoreReset on the impl aspect
//TICKLITE_CheckForExpiration on the impl aspect
//TICKLITE_OnExpiration
class TLGunFinalTickResolver : public UArtilleryDispatch::TL_ThreadedImpl /*Facaded*/
{
public:
	FSkeletonKey EntityKey;

	int cadence_ticks = 0;
	int cadence_interval = 16;
	TLGunFinalTickResolver(): TL_ThreadedImpl()
	{
	}

	TLGunFinalTickResolver(
		FSkeletonKey Target, const FArtilleryGun* Exists) : TL_ThreadedImpl(), EntityKey(Target), ExistCheck(Exists)
	{
	}
	
	void TICKLITE_StateReset()
	{
		//todo: need a good way to reset cadence. :/
	}
	
	void TICKLITE_Calculate()
	{
	}
	
	void TICKLITE_Apply()
	{
		// handle gun cooldown (fire rate)
		AttrMapPtr AttribMap = ADispatch->GetAttribMap(EntityKey);
		if (!AttribMap)
		{
			return;
		}
		
		AttrPtr GunCooldownRemaining = AttribMap->FindRef(COOLDOWN_REMAINING);

		// Reduce cd remaining by one tick, flooring at 0.
		if (GunCooldownRemaining.IsValid())
		{
			const float CurrCooldownValue = GunCooldownRemaining->GetCurrentValue();
			if (CurrCooldownValue > 0.f) {
				GunCooldownRemaining->SetCurrentValue(std::max(CurrCooldownValue - 1.f, 0.f));
			}
		}

		// handle reload
		AttrPtr CurrentAmmo = AttribMap->FindRef(AMMO);
		AttrPtr MaxAmmo = AttribMap->FindRef(MAX_AMMO);
		AttrPtr ReloadTime = AttribMap->FindRef(RELOAD);
		AttrPtr ReloadRemaining = AttribMap->FindRef(RELOAD_REMAINING);
		if (CurrentAmmo && MaxAmmo && ReloadTime && ReloadRemaining)
		{
			float CurrentAmmoValue = CurrentAmmo->GetCurrentValue();
			float MaxAmmoValue = MaxAmmo->GetCurrentValue();
			float ReloadTimeValue = ReloadTime->GetCurrentValue();
			float ReloadRemainingValue = ReloadRemaining->GetCurrentValue();
			if (MaxAmmoValue > 0.f)
			{
				// Only process reload system if this gun uses ammo + ammo is at zero
				if (ReloadRemainingValue > -1.f)
				{
					// Tick reload timer if its greater than -1, stopping at -1
					ReloadRemaining->SetCurrentValue(ReloadRemainingValue - 1.f);
				}

				if (ReloadRemainingValue == 0)
				{
					// Complete the reload
					CurrentAmmo->SetCurrentValue(MaxAmmoValue);
				}
		
				if (CurrentAmmoValue <= 0.f && ReloadRemainingValue <= -1.f)
				{
					// If we are out of ammo and the reload remaining is negative, start the reload
					ReloadRemaining->SetCurrentValue(ReloadTimeValue);
				}
			}
		}
	}
	
	void TICKLITE_CoreReset()
	{
	}

	bool TICKLITE_CheckForExpiration()
	{
		if (cadence_ticks%cadence_interval == 0)
		{
			if (ExistCheck == nullptr) //TODO: adjust to make this deterministic.
			{
				return true;
			}
		}
		++cadence_ticks;
		return false;
	}

	void TICKLITE_OnExpiration()
	{
		//no op
	}
private:
	const FArtilleryGun* ExistCheck;
};

typedef Ticklites::Ticklite<TLGunFinalTickResolver> GunFinalTickResolver;

