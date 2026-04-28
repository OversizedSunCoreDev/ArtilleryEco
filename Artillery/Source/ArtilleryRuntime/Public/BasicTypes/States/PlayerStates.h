#pragma once

#include "ConservedStates.h"
#include "PlayerStates.generated.h"

USTRUCT(BlueprintType)
struct ARTILLERYRUNTIME_API FPlayerStates : public FConservedStateData
{
	GENERATED_BODY()
	//I spent a bit thinking about autonumbering these and it's not actually that hard but I just...
	//let's please switch to gameplay tags for most stuff. it might still make sense to have a few of these
	//specifically for players just for the sake of extremely compact and extremely simple state management, but...
	constexpr static F DetailedGroundProxi		= F::F1;
	constexpr static F DetailedWallProxi		= F::F2;
	constexpr static F DetailedJumpState		= F::F3;
	constexpr static F DetailedWallClingState	= F::F4;
	constexpr static F DetailedWallKickState	= F::F5;
	constexpr static F DetailedDashState		= F::F6;
	
	unsigned long long Ground()
	{
		return GetField(DetailedGroundProxi);
	}
	
	void Ground(char val)
	{
		SetField(DetailedGroundProxi, val);
	}
	
	unsigned long long Wall()
	{
		return GetField(DetailedWallProxi);
	}
	
	void Wall(char val)
	{
		SetField(DetailedWallProxi, val);
	}
	
	unsigned long long MomentumModifyForWallProx()
	{
		return GetField(DetailedWallClingState);
	}
	
	void MomentumModifyForWallProx(char val)
	{
		SetField(DetailedWallClingState, val);
	}
	
	constexpr static char WallClingSpeedLimiting	= 1; //0001
	constexpr static char WallClingDelayFling		= 2; //0010
	constexpr static char WallHandlingChanged	= 4; //0100
	constexpr static char WallClingUncertain		= 8; //1000
	constexpr static char WallNone					= 0; //0000
	constexpr static char WallNear					= 1; //0001
	constexpr static char WallFar					= 2; //0010
	constexpr static char WallTouching				= 4; //0100
	constexpr static char WallContactPoor			= 8; //1000
	constexpr static char GroundNone				= 0; //0000
	constexpr static char GroundClose				= 1; //0001
	constexpr static char GroundTouching			= 2; //0010
	constexpr static char GroundContactPoor			= 4; //0100
	constexpr static char GroundSlanted				= 8; //1000
};