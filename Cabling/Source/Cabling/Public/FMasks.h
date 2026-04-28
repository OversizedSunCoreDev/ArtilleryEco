// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

// there's a set of internal constexprs because UE doesn't allow anything other than uint8, doesn't let you namespace enums,
// and is generally very weird about them. I'd originally wanted the enum to just be the bit patterns themselves,
// but you can't fit those in a uint8! so that's fun! That's cool!
UENUM(Blueprintable) enum class  E_ArtilleryIntents : uint8
{
	MenuIndex = 0,
	ViewIndex = 1,
	AIndex = 2,
	BIndex = 3,
	XIndex = 4,
	YIndex = 5,
	DUpIndex = 6,
	DDownIndex = 7,
	DLeftIndex = 8,
	DRightIndex = 9,
	LShoulderIndex = 10,
	RShoulderIndex = 11,
	LTriggerIndex = 12,
	RTriggerIndex = 13,
	StickFlickSpecialIndex = 14,
	
	LStickClickIndex= 15,
	RStickClickIndex= 16,
	NoIntent = 17,
};

namespace Arty
{
	namespace Intents{	
		// Buttons, please read:
		// 1 bit per button
		// in LOWEST TO HIGHEST order
		// Menu ,
		// View 
		// A,
		// B,
		// X,
		// Y,
		// DPadUp,
		// DPadDown,
		// DPadLeft,
		// DPadRight,
		// LeftShoulder,
		// RightShoulder,
		// LeftTrigger,
		// RightTrigger,
		typedef uint64 Intent;
		constexpr size_t TYPEBREAK_MAPPING_FROM_BC_BUTTONS = 20;
		constexpr Intent Menu =			0b1;
		constexpr Intent View =			0b10;
		constexpr Intent A =			0b100;
		constexpr Intent B =			0b1000;
		constexpr Intent X =			0b10000;
		constexpr Intent Y =			0b100000;
		constexpr Intent DUp =			0b1000000;
		constexpr Intent DDown =		0b10000000;
		constexpr Intent DLeft =		0b100000000;
		constexpr Intent DRight =		0b1000000000;
		constexpr Intent LShoulder =	0b10000000000;
		constexpr Intent RShoulder =	0b100000000000;
		constexpr Intent LTrigger =		0b1000000000000;
		constexpr Intent RTrigger =		0b10000000000000;
		constexpr Intent StickFlick =	0b100000000000000;
		constexpr Intent LStickClick =	0b1000000000000000;
		constexpr Intent RStickClick =	0b10000000000000000;

		constexpr uint8 MenuIndex = 0;
		constexpr uint8 ViewIndex = 1;
		constexpr uint8 AIndex = 2;
		constexpr uint8 BIndex = 3;
		constexpr uint8 XIndex = 4;
		constexpr uint8 YIndex = 5;
		constexpr uint8 DUpIndex = 6;
		constexpr uint8 DDownIndex = 7;
		constexpr uint8 DLeftIndex = 8;
		constexpr uint8 DRightIndex = 9;
		constexpr uint8 LShoulderIndex = 10;
		constexpr uint8 RShoulderIndex = 11;
		constexpr uint8 LTriggerIndex = 12;
		constexpr uint8 RTriggerIndex = 13;
		constexpr uint8 StickFlickSpecialIndex = 14;
		constexpr uint8 LStickClickIndex = 15;
		constexpr uint8 RStickClickIndex = 16;
		//3 unused bits follow.
	}
}
