// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

/**
 * Component for managing input streams in an ECS-like way, where any controller can request any stream.
 */
#include "FActionBitMask.h"
#include "ActionPatternKey.h"
#include "BristleconeCommonTypes.h"
#include "Containers/TripleBuffer.h"
#include "SkeletonTypes.h"

DECLARE_DELEGATE_TwoParams(FArtilleryAddEnemyToControllerSubsystem, const ActorKey ThistleInject, uint64_t CurrentTick)

enum SKLiveness
{
	ALIVE,
	DEAD,
	UNKNOWN,
	TOMB
};

UENUM(BlueprintType, Blueprintable)
enum class E_PlayerKEY : uint8
{
	NONE = 0,
	CABLE = 1,
	ECHO = 2,
	TWO = 20,
	THREE = 30,
	FOUR = 40,
	FIVE = 50,
	SIX = 60
	//6p max, bb
};

namespace Arty
{
	typedef TheCone::PacketElement INNNNCOMING;
	typedef uint32_t InputStreamKey;
	typedef E_PlayerKEY APlayer;
	typedef APlayer PlayerKey;
	typedef uint32_t FireControlKey;
	// ReSharper disable once CppRedundantQualifier
	constexpr uint32_t ArtilleryTickHertz = TheCone::LongboySendHertz;
}

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "Engine/DataTable.h"
#include "Containers/CircularBuffer.h"
#include "FGunKey.h"
// ReSharper disable once CppUnusedIncludeDirective
#include <EAttributes.h> //if you remove this, you will have a very bad time.

namespace Arty
{
	typedef uint64_t ArtilleryDataSetKey;
	typedef ArtilleryDataSetKey ADSKey;
	typedef uint64_t TickliteKey;
	//this must use the same type as actor keys and artillery object keys like  projectile or mesh
	
	DECLARE_DELEGATE(CalculateTicklite);
	//performs the actual data transformations.
	DECLARE_DELEGATE(ApplyTicklite);
	//resets any data related to apply
	DECLARE_DELEGATE(ResetTicklike)
	DECLARE_DELEGATE_TwoParams(NormalTicklite, ActorKey, ADSKey);
	
	// direct use of FINAL_TICK_RESOLVE is strictly prohibited and will break everything.
	// it is possible in modern C++ to create a 2nd enum that hides recharge.
	// I would like to not do this. please don't use FINAL_TICK_RESOLVE directly.
	// FINAL_TICK_RESOLVE is ONLY for FTFinalTickResolver or a subclass and is reserved
	// for gameplay implementation of tick-resolving actions like finalizing damage
	// or applying queued effects.
	enum TicklitePhase
	{
		Early =  1,
		Normal = 2,
		Late = 4,
		PASS_THROUGH = 1024, //this is reserved for the passthrough ticklite.
		FINAL_TICK_RESOLVE = 2048
	};
#define TICKLITEPHASESCOUNT 5
	enum TickliteCadence
	{
		Critical = 1,
		Tick = 2,
		Lite = 8,
		Slow = 32
	};

	struct TicklikeMemoryBlock
	{
		TickliteCadence Cadence = TickliteCadence::Lite;
		TicklitePhase RunGroup = TicklitePhase::Normal;
		ArtilleryTime MadeStamp = 0;
	};
	
	struct TicklitePrototype : TicklikeMemoryBlock
	{
		virtual void CalculateTickable() = 0;
		virtual bool ShouldExpireTickable() = 0;
		
		//This trigger effects and this is borderline necessary for some semantics, like popping a poison tag off
		//when an effect ends. where possible, we should prefer ArtilleryEvents for this, but it won't always be viable.
		//use your best judgment.
		virtual bool OnExpireTickable() = 0;
		virtual void ApplyTickable() = 0;
		virtual void ReturnToPool() = 0;

		virtual ~TicklitePrototype()
		{
		};
	};

	//You might notice Ticklite is the name used in implementation, but you might derive other ticklikes.
	//In fact, this class exists as what is, effectively, a poor man's trait. This will be deprecated eventually
	//if we don't find any uses for it, and the ticklite template will supersede it, but I think that won't happen.

	struct EventBufferInfo
	{
		FGunKey GunKey;
		ArtIPMKey Action;
		FActionBitMask ActionBitMask;

		bool operator<(const EventBufferInfo& Other) const
		{
			return GunKey < Other.GunKey;
		}

		static EventBufferInfo Default()
		{
			EventBufferInfo EBI;
			EBI.GunKey = FGunKey::Invalid();
			EBI.Action = InternallyStateless;
			EBI.ActionBitMask = FActionBitMask::Default();
			return EBI;
		}
	};

	typedef TArray<TPair<BristleTime,EventBufferInfo>> EventBuffer;
	typedef TTripleBuffer<EventBuffer> BufferedEvents;

	//Ever see the motto of the old naval railgun project? I won't spoil it for you.
	typedef FVector3d VelocityVec;
	typedef TTuple<ArtilleryTime, FSkeletonKey, VelocityVec> VelocityEvent;
	
	typedef TCircularQueue<VelocityEvent> VelocityStack;
	typedef TSharedPtr<VelocityStack> VelocityEP; //event pump, if you must know.
}

//PATH TO DATA TABLES
//constexpr const FString GunsManifests = "";
//TODO: ALWAYS customize this to the sample-rate you select for cabling. ALWAYS. Or your game will feel Real Bad.
constexpr int ArtilleryInputSearchWindow = TheCone::BristleconeSendHertz;
constexpr inline int ArtilleryHoldSweepBack = 5; // this is literally the sin within the beast. 
constexpr inline int ArtilleryFlickSweepBack = 15; // And this is no better.

using namespace Arty;
struct FActionPatternParams
{
	//this specifies a parametric bind's "preference" which will need to become an int for priority.
	//and if the binding consumes the input or passes it through.
	// an example is that we WILL want to say that holding down the trigger should be fired before
	// single press. actually, we might do pattern-priority rather than anything else.
	// hard to say. there is a trick here that could let us handle firing a diff ability if the ability is on cool down but I'm not borrowing trouble.
	bool preferToMatch = false;
	bool consumeInput = true;
	bool defaultBehavior = false;
	bool FiresCosmetics = false;

	FGunKey ToFire; //IT WAS NOT A MISTAKE. I AM A GENIUS.
	FActionBitMask ToSeek;
	InputStreamKey MyInputStream;
	FireControlKey MyOrigin;
	FActionPatternParams(const FActionBitMask ToSeek_In, FireControlKey MyOrigin_In, InputStreamKey MyInputStream_In, FGunKey Fireable)
	{
		ToFire = Fireable;
		ToSeek = ToSeek_In;
		MyInputStream = MyInputStream_In;
		MyOrigin = MyOrigin_In;
	}

	friend uint32 GetTypeHash(const FActionPatternParams& Other)
	{
		return HashCombineFast(GetTypeHash(Other.ToFire), Other.ToSeek.buttons.to_ulong());
	}

	FORCEINLINE bool operator==(const FActionPatternParams& Other) const
	{
		return ToFire == Other.ToFire && MyInputStream == Other.MyInputStream && ToSeek.buttons.to_ulong() == Other.ToSeek.buttons.to_ulong();
	}
};

static bool operator==(FActionPatternParams const& lhs, FActionPatternParams const& rhs) {
	return lhs.ToFire == rhs.ToFire;
}
