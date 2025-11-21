#pragma once
#include "CoreMinimal.h"
#include "ConservedAttribute.h"

#include "ConservedKey.h"
#include "ConservedVector.h"
#include "EAttributes.generated.h"

UENUM(BlueprintType, Blueprintable)
enum class E_AttribKey : uint8
{
	// Entity Attributes
	Speed,
	Health,
	MaxHealth,
	HealthRechargePerTick,
	Shields,
	MaxShields,
	ShieldsRechargePerTick,
	Mana,
	MaxMana,
	ManaRechargePerTick,
	TicksTilJumpAvailable,
	JumpsAvailable,
	JumpHeight,
	ProposedDamage,
	IsLockedOn,
	IsActive,
	
	// Gun Attributes
	Ammo,
	MaxAmmo,
	FireCooldown,
	FireCooldownRemaining,
	ReloadTime,
	ReloadTimeRemaining,
	Range,
	TicksSinceLastFired,
	LastFiredTimestamp,
	TriggerPulled,
	
	Height
};

UENUM(BlueprintType, Blueprintable)
enum class E_IdentityAttrib : uint8
{
	Target,
	EquippedMainGun,
	EquippedSecondaryGun,
	EquippedMoveAbility,
	EquippedDefAbility,
	EquippedDashAbility,
	CurrentCharacter,
	CurrentController,
	MainGunModel,
	Squad,
	Self
};

UENUM(BlueprintType, Blueprintable)
enum class E_VectorAttrib : uint8
{
	//Where the gun points. This one's sort of a problem atm.
	AimVector,
	// Where control vector points in UE. This should only be used for error correction and control remediation.
	ChaosControlVector,
	// This is our actual understanding of the control vector, and represents canonical truth.
	ArtInputDeltaUnitVector,
	// This represents the system's true understanding of the current look rotator.
	// Normally only present for players! For enemies, expect to use _AIM VECTOR_
	TrueLookVector,
	//This vector doesn't include changes made by the look state machine of the character, only the camera manager and controller object.
	UControllerOnlyLookVector,
	// The Artillery's 3-euler understanding of where the facing should be. under the hood, we actually use quats for this.
	// but UE kinda expects rotators. This mostly comes up in case we need to rediscipline the gamestate from a simulating server to recover from a deep data desync.
	FacingVector,
	Velocity,
	Forces,
	//this is currently used in the bristle game mode for a hack to fix a weird spawn behavior that's caused by lifecycle issues.
	InitialPositionVec,
	InitialRotationVec,
	Destination,
	Location //GENERALLY AT LEAST ONE TICK OLD! USE THE ESTIMATOR WITH IT!!!!!!
};

namespace Arty
{
	//gonna need to ditch the stupid attributes from gameplay or rework them
	//all because of the bloody Base value meaning that we might have 2 doubles per.
	//if you want a base, add a base.
	//Attributes is a UE namespace, so we gotta call this attributeslist. sigh.
	//unfortunately, I don't remember the plan for this. it'll come to me. --jmk
	namespace AttributesList{

	}
	
	//MANA should always be granted in multiples of 10 since 10m/t is our standard recharge.
	using AttribKey = E_AttribKey;
	using Attr = AttribKey;
	using Ident = E_IdentityAttrib;
	
	typedef E_IdentityAttrib FARelatedBy;
	typedef E_VectorAttrib Attr3;

	// Entity attribs
	constexpr AttribKey HEALTH = Arty::AttribKey::Health;
	constexpr AttribKey MAXHEALTH = Arty::AttribKey::MaxHealth;
	constexpr AttribKey MANA = Arty::AttribKey::Mana;
	constexpr AttribKey DASH_CURRENCY = MANA;
	constexpr AttribKey MAXMANA = Arty::AttribKey::MaxMana;
	constexpr AttribKey PROPOSED_DAMAGE = Arty::AttribKey::ProposedDamage;

	// Gun Attribs
	constexpr AttribKey AMMO = Arty::AttribKey::Ammo;
	constexpr AttribKey MAX_AMMO = Arty::AttribKey::MaxAmmo;
	constexpr AttribKey COOLDOWN = Arty::AttribKey::FireCooldown;
	constexpr AttribKey COOLDOWN_REMAINING = Arty::AttribKey::FireCooldownRemaining;
	constexpr AttribKey RELOAD = Arty::AttribKey::ReloadTime;
	constexpr AttribKey RELOAD_REMAINING = Arty::AttribKey::ReloadTimeRemaining;
	constexpr AttribKey TICKS_SINCE_GUN_LAST_FIRED = Arty::AttribKey::TicksSinceLastFired;
	constexpr AttribKey TRIGGER_PULLED = Arty::AttribKey::TriggerPulled;
	typedef TSharedPtr<FConservedAttributeData> AttrPtr;
	typedef TSharedPtr<FConservedAttributeKey> IdentPtr;
	typedef TSharedPtr<FConservedVector> Attr3Ptr;
	typedef TMap<AttribKey, AttrPtr> AttributeMap;
	typedef TMap<Ident, IdentPtr> IdentityMap;
	typedef TMap<Attr3, Attr3Ptr> Attr3Map;
	typedef TSharedPtr<AttributeMap> AttrMapPtr;
	typedef TSharedPtr<IdentityMap> IdMapPtr;
	typedef TSharedPtr<Attr3Map> Attr3MapPtr;
}

#if ENABLE_VISUAL_LOG
#define VISLOG_ATTRIBUTE(artillery_dispatch, key, attribute) if (FVisualLogger::IsRecording()) UE_VLOG(GetOwner(), LogPawnAction, Log, TEXT("%s> " #attribute ": %f"),*GetName(), artillery_dispatch->GetAttr(key, attribute)->CurrentValue)
#define VISLOG_VEC_ATTRIBUTE(artillery_dispatch, key, attribute) if (FVisualLogger::IsRecording()) UE_VLOG(GetOwner(), LogPawnAction, Log, TEXT("%s> " #attribute ": %s"),*GetName(), *artillery_dispatch->GetVecAttr(key, attribute)->CurrentValue.ToString())
#endif // ENABLE_VISUAL_LOG