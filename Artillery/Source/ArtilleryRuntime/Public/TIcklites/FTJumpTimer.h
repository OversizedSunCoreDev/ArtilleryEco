#pragma once

#include "Ticklite.h"
#include "ArtilleryDispatch.h"
#include "FArtilleryTicklitesThread.h"

class FTJumpTimer : public UArtilleryDispatch::TL_ThreadedImpl
{
private:
	FSkeletonKey JumpTarget;

public:
	FTJumpTimer() : TL_ThreadedImpl()
	{
	}

	FTJumpTimer(FSkeletonKey Jumper) : TL_ThreadedImpl(), JumpTarget(Jumper)
	{
	}

	void TICKLITE_StateReset()
	{
	}

	void TICKLITE_Calculate()
	{
	}
	
	void TICKLITE_Apply()
	{
		auto ticksLeftPtr = ADispatch->GetAttrib(JumpTarget, Attr::TicksTilJumpAvailable);
		if (!ticksLeftPtr)
		{
			return;
		}
		if (ticksLeftPtr->GetCurrentValue() >= 1)
		{
			ticksLeftPtr->AddToCurrentValue(-1);
		}
	}

	void TICKLITE_CoreReset() {
	}

	bool TICKLITE_CheckForExpiration() {
		auto ticksLeftPtr = ADispatch->GetAttrib(JumpTarget, Attr::TicksTilJumpAvailable);

		// If there is no valid ptr, expire
		if (!ticksLeftPtr)
		{
			return true;
		}

		return ticksLeftPtr->GetCurrentValue() <= 0;
	}

	void TICKLITE_OnExpiration() {
	}
};

typedef Ticklites::Ticklite<FTJumpTimer> TL_JumpTimer;