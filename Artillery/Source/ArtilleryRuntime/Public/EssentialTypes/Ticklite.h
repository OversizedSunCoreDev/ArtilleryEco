#pragma once

#include "ArtilleryCommonTypes.h"

namespace Ticklites
{
	//conserved attributes mean that we always have a shadow copy ready.
	// a successful Calculate function should reference the attribute not by most recent, but by exact index.
	//good support for this isn't in yet, but during our early work, the conserved attributes will still
	//protect us from partial memory commits and similar that might cause truly weird bugs.
	//instead, right now, we just run tickables "across" ticks right now.
	//This conforms with our measure, cut, fit, finish policy, as this is still in the _measure_ phase.
	
	template <typename YourImplementation> 
	struct Ticklite : public TicklitePrototype 
	{
		using Ticklite_Impl = YourImplementation;
		YourImplementation Core;
		
		virtual void CalculateTickable() override
		{
			Core.TICKLITE_StateReset();
			//as always, the use of keys over references will make rollback far far easier.
			//when performing operations here, do not expect floating point accuracy above 16 ulps.
			//If you do, you will get fucked sooner or later. I guarantee it.
			Core.TICKLITE_Calculate();
		}

		virtual void ApplyTickable() override
		{
			Core.TICKLITE_Apply();
		}

		virtual void ReturnToPool() override
		{
			Core.TICKLITE_CoreReset();
			Core.TICKLITE_StateReset();
		}

		//expiration will likely get factored out into a delegate or pushed into the TL_Impl
		//to help ensure that we don't end up with 20 million tickables, each of which expires in a slightly different way.
		virtual bool ShouldExpireTickable() override
		{
			return Core.TICKLITE_CheckForExpiration();
		}

		//expiration will likely get factored out into a delegate or pushed into the TL_Impl
		//to help ensure that we don't end up with 20 million tickables, each of which expires in a slightly different way.
		virtual bool OnExpireTickable() override
		{
			Core.TICKLITE_OnExpiration();
			return true;
		}

		virtual ~Ticklite() override
		{
		}

		//You may wish to implement a ticklite that uses reference counting.
		//this DOES NOT. Ticklites and their implementations are assumed to have different lifecycles,
		//with the ticklite either dying at the same time, or dying first. Anything besides this will cause bugs.
		//this can be optimized to remove the copy operation with a little work. once I'm sure this is the FINAL FORM
		//I'll do the work.
		Ticklite(Ticklite_Impl ImplInstance)
		{
			Core = ImplInstance;
		}
	};
}

namespace Arty
{
	typedef TPair<TSharedPtr<TicklitePrototype>, TicklitePhase> StampLiteRequest;
	typedef TArray< TSharedPtr<TicklitePrototype>> TickliteGroup;
	typedef TCircularQueue<StampLiteRequest> TickliteRequests;
	typedef TSharedPtr<TCircularQueue<StampLiteRequest>> TickliteBuffer;
}
