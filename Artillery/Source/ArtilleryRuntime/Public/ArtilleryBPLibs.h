// ReSharper disable CppRedundantParentheses
#pragma once

#include <GunOwner.h>

#include "ArtilleryShell.h"
#include "AtomicTagArray.h"
#include "ArtilleryBPLibs.generated.h"

struct FSoundPinBag;
class UArtilleryGameplayTagContainer;
class UBarragePlayerAgent;
class UCanonicalInputStreamECS;


UENUM()
enum class EArtilleryFindResult : uint8
{
	Found,
	NotFound
};


/** 
 * This function library distinguishes functions that are safe to call from non-artillery contexts for things like UI visuals
 * Unlike the other function libraries you can readily obtain the local player's information
 * This should be preferred for anything on the unreal gamethread talking to artillery, even though some artillery API calls are safe from any thread
 * 
 * Intended to be mainly used from blueprint
 */

UCLASS()
class ARTILLERYRUNTIME_API UArtilleryReadOnlyAccessLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	
	// Safely reads an attribute in a way that is safe to do outside of Artillery threads (useful for UI code)
	UFUNCTION(BlueprintPure, meta=(WorldContext = "WorldContextObject"), Category="Artillery|ReadOnly|Attributes")
	static bool ReadAttribute(const UObject* WorldContextObject, FSkeletonKey Owner, E_AttribKey Attrib, float& OutAttribute);
	
	// Safely reads a local player's attribute in a way that is safe to do outside of Artillery threads (useful for UI code)
	UFUNCTION(BlueprintPure, meta=(WorldContext = "WorldContextObject"), Category="Artillery|ReadOnly|Attributes")
	static bool ReadLocalPlayerAttribute(const UObject* WorldContextObject, E_AttribKey Attrib, float& OutAttribute);
	
	// Safely reads a local player's vector attribute in a way that is safe to do outside of Artillery threads (useful for UI code)
	UFUNCTION(BlueprintPure, meta=(WorldContext = "WorldContextObject"), Category="Artillery|ReadOnly|Attributes")
	static bool ReadLocalPlayerVectorAttribute(const UObject* WorldContextObject, E_VectorAttrib VectorAttrib, FVector& OutAttribute);
	
	// Safely reads a local player's identity attribute in a way that is safe to do outside of Artillery threads (useful for UI code)
	UFUNCTION(BlueprintPure, meta=(WorldContext = "WorldContextObject"), Category="Artillery|Keys")
	static bool ReadLocalPlayerIdentity(const UObject* WorldContextObject, E_IdentityAttrib IdentityAttrib, FSkeletonKey& OutIdentity);
	
	
	// Safely obtains the local player's skeleton key (useful for UI code)
	UFUNCTION(BlueprintPure, meta=(WorldContext = "WorldContextObject"), Category="Artillery|ReadOnly")
	static FSkeletonKey GetLocalPlayerKey(const UObject* WorldContextObject);
};


UCLASS(meta=(ScriptName="InputSystemLibrary"))
class ARTILLERYRUNTIME_API UInputECSLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static void GetHistoricalInputs(UCanonicalInputStreamECS* InputECS, TArray<FArtilleryShell>& Inputs, int Count);

	UFUNCTION(BlueprintPure, meta = (ScriptName = "Get15PlayerInputs", DisplayName = "Get Last 15 of Local Player's Inputs", WorldContext = "WorldContextObject", HidePin = "WorldContextObject"),  Category="Artillery|Inputs")
	static void K2_Get15LocalHistoricalInputs(UCanonicalInputStreamECS* InputECS, TArray<FArtilleryShell> &Inputs);
};

UCLASS(meta=(ScriptName="ArtillerySystemLibrary"))
class ARTILLERYRUNTIME_API UArtilleryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	
	// Newer BP api that saves users some time by avoiding needing to bring their own dispatch pointer
	// these will only work when in the scope of FArtilleryBPCallThreadLocalScope. You probably don't want to call these from raw C++
	// arguably should be protected but I have found that assumption tends to not help
	
	UFUNCTION(BlueprintCallable, meta = (ExpandEnumAsExecs = "ReturnValue"), Category="Artillery|Attributes")
	static EArtilleryFindResult K2_GetLocation(FSkeletonKey Owner, FVector& OutLocation);

	UFUNCTION(BlueprintCallable, meta = (ExpandEnumAsExecs = "ReturnValue"), Category="Artillery|Attributes")
	static EArtilleryFindResult K2_GetAttribute(FSkeletonKey Owner, E_AttribKey Attrib, float& OutAttribute);
	
	UFUNCTION(BlueprintCallable, meta = (ExpandEnumAsExecs = "ReturnValue"), Category="Artillery|Attributes")
	static EArtilleryFindResult K2_GetVectorAttribute(FSkeletonKey Owner, E_VectorAttrib Attrib, FVector& OutVectorAttribute);
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetRelatedKey", DisplayName = "Get Related Key From", ExpandEnumAsExecs="ReturnValue"), Category="Artillery|Keys")
	static EArtilleryFindResult K2_GetIdentity(FSkeletonKey Owner, E_IdentityAttrib Attrib, FSkeletonKey& OutIdentity);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Play Selected Sound At Location", ExpandEnumAsExecs = "ReturnValue"),  Category="Artillery|Inventory|Effects")
	static EArtilleryFindResult K2_PlaySoundEffectAt(const FSoundPinBag& SoundName, FSkeletonKey Source, const FVector& Location);
	
	
	/**
	 * Draw a debug line for one frame. Safe to call from any thread (time currently does nothing, placeholder)
	 */
	UFUNCTION(BlueprintCallable,  Category="Artillery|Utils")
	static void K2_DrawDebugLine(const FVector& Start,const FVector& End, FColor Color = FColor::Red, float Time = 0.0f);
	
	/**
	 * Draw a debug sphere for one frame. Safe to call from any thread (time currently does nothing, placeholder)
	 */
	UFUNCTION(BlueprintCallable,  Category="Artillery|Utils")
	static void K2_DrawDebugSphere(const FVector& Location, FColor Color = FColor::Red, float Radius = 0.0f, float Time = 0.0f);
	
	UFUNCTION(BlueprintCallable,  Category="Artillery|Utils")
	static void K2_DrawDebugString(const FVector& Location, const FString& String, FColor Color = FColor::Red);
	
	UFUNCTION(BlueprintCallable,  Category="Artillery|Utils")
	static void K2_PrintDebugString(const FString& String, bool bOnScreen = true, bool bLog = true, float Time = 0.0f, FColor Color = FColor::Red, int32 Key = -1);
	
	/**
	 *Get Any Player's vector Attribute
	 * @param OutVector The vector value (NAN if not found)
	 * @return If the vector attribute was found
	 */
	UFUNCTION(BlueprintCallable, meta = (ExpandEnumAsExecs = "ReturnValue"),  Category="Artillery|Attributes")
	static EArtilleryFindResult K2_GetPlayerVector(UArtilleryDispatch* Dispatch, E_VectorAttrib Attrib, E_PlayerKEY Player, FVector& OutVector);

public:
	static int32 GetTotalsTickCount(UArtilleryDispatch* MyDispatch);
	
	// Checks TransformDispatch For a Location
	static FVector GetLocation(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, bool& bFound);
	
	static Attr3Ptr GetAttr3Ptr(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, E_VectorAttrib Attrib);

	static void RequestUnboundGun(UArtilleryDispatch* Dispatch, FARelatedBy Relationship, const FSkeletonKey& Requester, const FGunKey& GunKey);

	//TODO: allow guns to fire in the past?
	static void RequestGunFire(UArtilleryDispatch* Dispatch, const FGunKey& GunKey);

	//TODO: This takes a key, gets the location from the ATTRIBUTE, and then shifts it up to represent a good centroid target point

	static FVector GetPlayerLocationAsEstTarget(UArtilleryDispatch* Dispatch, E_PlayerKEY Player);

	/**
	 * @param OutAttribute The attribute value (NAN if not found)
	 * @return If the attribute was found
	 */
	static bool GetAttribute(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, E_AttribKey AttributeKey, float& OutAttribute);
	
	static FSkeletonKey GetIdentity(UArtilleryDispatch* Dispatch,  FSkeletonKey Owner, E_IdentityAttrib Attrib, bool& bFound);

	static bool PlaySoundEffectAt(UArtilleryDispatch* Dispatch, const FSoundPinBag& SoundName, FSkeletonKey Source, const FVector& Location);
	
	/**
	 *Get Any Player's vector Attribute
	 * @param OutVector The vector value (NAN if not found)
	 * @return If the vector attribute was found
	 */
	static bool GetPlayerVector(UArtilleryDispatch* Dispatch, E_VectorAttrib Attrib, E_PlayerKEY Player, FVector& OutVector);

	/**
	 *Get Any Player's Attribute
	 * @param OutAttribute The attribute value (NAN if not found)
	 * @return If the attribute was found
	 */
	UFUNCTION(BlueprintCallable, meta = (ExpandBoolAsExecs="ReturnValue"), Category="Artillery|Attributes")
	static bool GetPlayerAttribute(UArtilleryDispatch* Dispatch, E_AttribKey Attrib, E_PlayerKEY Player, float& OutAttribute);

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ApplyDamage", DisplayName = "Apply Damage", ExpandBoolAsExecs="ReturnValue"),  Category="Artillery|Attributes")
	static bool ApplyDamage(UArtilleryDispatch* Dispatch, const FSkeletonKey Target, float DamageToApply, const FVector& SourceLocation = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetGameplayTagsByKey", DisplayName = "Get Gameplay Tags for Key", ExpandBoolAsExecs="bFound", WorldContext = "WorldContextObject", HidePin = "WorldContextObject"),  Category="Artillery|Tags")
	static UArtilleryGameplayTagContainer* GetTagsByKey(UArtilleryDispatch* Dispatch, FSkeletonKey Key, bool& bFound);

	static FConservedTags InternalTagsByKey(UArtilleryDispatch* Dispatch, FSkeletonKey Key, bool& bFound);


	//DEPRECATED
	//TODO: WARNING SERIOUS DETERMINISM RISK. THIS REFERENCES CHAOS AND IS CALLED ALL OVER THE PLACE.
	static void GetPlayerVectors(UArtilleryDispatch* Dispatch, FVector& Forward, FVector& Right);

	static void SimpleEstimator(UCanonicalInputStreamECS* ptr, FVector& Forwardish, double Counter = 15);

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetPlayerDirectionEstimator", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", DisplayName = "Get Local Player's Direction Estimator"),  Category="Artillery|Character")
	static void K2_GetPlayerDirectionEstimator(UObject* WorldContextObject, FVector& Forward);


	// Kills a projectile without asking enough questions. True if found
	// differs from tombstone primitive ONLY in that it will NOT kill non-projectiles.
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Kills a projectile without asking enough questions. True if found."),  Category="Artillery|Physics")
	static bool SafelyDeleteProjectile(UArtilleryDispatch* Dispatch, FSkeletonKey Target);

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "TombstoneAPrimitive", DisplayName = "Kills a primitive without asking enough questions. True if found."),  Category="Artillery|Physics")
	static bool TombstonePrimitive(UArtilleryDispatch* Dispatch, FSkeletonKey Target);

	UFUNCTION(BlueprintCallable, Category = "SkeletonTypes")
	static void BreakSkeletonKey(const FSkeletonKey& Key, int32& KeyValue);


	static PlayerKey GetPlayerKeyFromSkeletonKey(UCanonicalInputStreamECS* InputECS, const FSkeletonKey& Key);


	[[deprecated("The idea of distinguishing ONE local player in the artillery sim which is different per-player is very unsafe and actors are extremely unsafe to use off of the game thread")]]
	static PlayerKey GetLocalPlayerKey(UWorld* World);

	[[deprecated("The idea of distinguishing ONE local player in the artillery sim which is different per-player is very unsafe and actors are extremely unsafe to use off of the game thread")]]
	//This is bad practice but no longer unsafe. because it is now WORLD AWARE, it will select the local player for the
	//current world. you really really should not use it, but it now does the correct thing.
	static UBarragePlayerAgent* GetLocalPlayerBarrageAgent(UArtilleryDispatch* Dispatch);

	[[deprecated("The idea of distinguishing ONE local player in the artillery sim which is different per-player is very unsafe and actors are extremely unsafe to use off of the game thread")]]
	static FSkeletonKey GetLocalPlayerKey_LOW_SAFETY(UCanonicalInputStreamECS* InputECS);

	[[deprecated("The idea of distinguishing ONE local player in the artillery sim which is different per-player is very unsafe and actors are extremely unsafe to use off of the game thread")]]
	//we need a better and less dangerous idiom than this, you were right, @Maslab.
	static AActor* GetLocalPlayer_UNSAFE(UArtilleryDispatch* Dispatch);
};
