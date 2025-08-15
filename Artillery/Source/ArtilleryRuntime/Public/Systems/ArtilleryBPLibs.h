// ReSharper disable CppRedundantParentheses
#pragma once

#include <GunOwner.h>

#include "ArtilleryDispatch.h"
#include "ArtilleryProjectileDispatch.h"
#include "CanonicalInputStreamECS.h"
#include "FTTimer.h"
#include "PhysicsTypes/BarragePlayerAgent.h"
#include "UArtilleryGameplayTagContainer.h"
#include "UFireControlMachine.h"
#include "Components/TimerTickliteHandlerComponent.h"
#include "ArtilleryBPLibs.generated.h"

UCLASS(meta=(ScriptName="InputSystemLibrary"))
class ARTILLERYRUNTIME_API UInputECSLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static void GetHistoricalInputs(TArray<FArtilleryShell>& Inputs, int Count)
	{
		UCanonicalInputStreamECS* ptr = UCanonicalInputStreamECS::SelfPtr;
		if(ptr)
		{
			InputStreamKey streamkey = ptr->GetStreamForPlayer(PlayerKey::CABLE);
			TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> sptr = ptr->GetStream(streamkey);
			for(int InputIndex = 0; InputIndex <= Count; ++InputIndex)
			{
				std::optional<FArtilleryShell> input =  sptr.Get()->peek( sptr->GetHighestGuaranteedInput() - InputIndex);
				Inputs.Add(input.has_value() ? input.value() : FArtilleryShell());
			}
		}
	}
	
	UFUNCTION(BlueprintPure, meta = (ScriptName = "Get15PlayerInputs", DisplayName = "Get Last 15 of Local Player's Inputs", WorldContext = "WorldContextObject", HidePin = "WorldContextObject"),  Category="Artillery|Inputs")
	static void K2_Get15LocalHistoricalInputs(UObject* WorldContextObject, TArray<FArtilleryShell> &Inputs)
	{
		GetHistoricalInputs(Inputs, 15);
	}
};

UCLASS(meta=(ScriptName="ArtillerySystemLibrary"))
class ARTILLERYRUNTIME_API UArtilleryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	//DEPRECATED
	//TODO: replace with something non-monotonic but still cadenced. aw jeez.
	static int32 GetTotalsTickCount()
	{
		return UArtilleryDispatch::SelfPtr ? UArtilleryDispatch::SelfPtr->ArtilleryAsyncWorldSim.SeqNumber / 4 : 0;
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetLocFromTransformDispatchIfAny", DisplayName = "Checks TransformDispatch For A Location", ExpandBoolAsExecs="bFound"), Category="Artillery|Attributes")
	static FVector K2_GetBarrageLocIfAny(FSkeletonKey Owner, bool& bFound)
	{
		bFound = false;
		return implK2_GetLocation(Owner, bFound);
	}
	
	static FVector implK2_GetLocation(FSkeletonKey Owner, bool& bFound)
	{
		bFound = false;
		if(UArtilleryDispatch::SelfPtr)
		{
			//GOD LIKE POWER
			FBLet Fiblet = UArtilleryDispatch::SelfPtr->GetFBLetByObjectKey(Owner, UArtilleryDispatch::SelfPtr->GetShadowNow());
			if(Fiblet)
			{
				FVector3f PantsWettingTerror = FBarragePrimitive::GetPosition(Fiblet);
				//make sure they aren't a NANdere lol
				if (!PantsWettingTerror.ContainsNaN())
				{
					bFound = true;
					return FVector(PantsWettingTerror);
				}
			}
		}
		bFound = false;
		return FVector( NAN); // YOU SHOULD NOT HAVE COME HERE
	}
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetAttribute", DisplayName = "Get Attribute Of", ExpandBoolAsExecs="bFound"), Category="Artillery|Attributes")
	static float K2_GetAttrib(FSkeletonKey Owner, E_AttribKey Attrib, bool& bFound)
	{
		bFound = false;
		return implK2_GetAttrib(Owner,Attrib, bFound);
	}
	
	static Attr3Ptr implK2_GetAttr3Ptr(FSkeletonKey Owner, E_VectorAttrib Attrib, bool& bFound)
	{
		bFound = false;
		if(UArtilleryDispatch::SelfPtr)
		{
			Attr3Ptr AttribVecPtr = UArtilleryDispatch::SelfPtr->GetVecAttr( Owner, Attrib);
			if(AttribVecPtr != nullptr)
			{
				bFound = true;
				return AttribVecPtr;
			}
		}
		return nullptr;
	}

	static void RequestUnboundGun(FARelatedBy Relationship, const FSkeletonKey& Requester, const FGunKey& GunKey)
	{
		UArtilleryDispatch* ptr = UArtilleryDispatch::SelfPtr;
		if(ptr)
		{
			TSharedPtr<F_INeedA> RouterHold = ptr->RequestRouter;
			if(RouterHold)
			{
				RouterHold->NewUnboundGun(Requester, GunKey, Relationship, ptr->GetShadowNow());	
			}
		}
	}

	//TODO: allow guns to fire in the past?
	static void RequestGunFire(const FGunKey& GunKey)
	{
		UArtilleryDispatch* ptr = UArtilleryDispatch::SelfPtr;
		if(ptr)
		{
			TSharedPtr<F_INeedA> RouterHold = ptr->RequestRouter;
			if(RouterHold)
			{
				RouterHold->GunFired(GunKey, UArtilleryDispatch::SelfPtr->GetShadowNow());
			}
		}
	}

	//TODO: This takes a key, gets the location from the ATTRIBUTE, and then shifts it up to represent a good centroid target point
	static FVector GetPlayerLocationAsEstTarget(E_PlayerKEY Player)
	{
		UArtilleryDispatch* ptr = UArtilleryDispatch::SelfPtr;
		if(ptr)
		{
			bool bFound  = false;
			FVector Value = FVector::ZeroVector;
			GetAnyPlayerVector(ptr->GetWorld(), E_VectorAttrib::Location, Player, bFound, Value);
			
			if (bFound && !Value.IsNearlyZero())
			{
				float height = 0.0;
				GetAnyPlayerAttrib(ptr->GetWorld(), E_AttribKey::Height, Player, bFound, height);
				if (bFound)
				{
					FVector Dir = FVector();
					K2_GetPlayerDirectionEstimator(Dir);
					return Value + Dir + (FVector::UpVector * height);
				}
			}
		}
		return FVector();
	}

	static float implK2_GetAttrib(FSkeletonKey Owner, E_AttribKey Attrib)
	{
		if(UArtilleryDispatch::SelfPtr)
		{
			AttrPtr Attribute = UArtilleryDispatch::SelfPtr->GetAttrib(Owner, Attrib);
			if(Attribute.IsValid())
			{
				return Attribute->GetCurrentValue();
			}
		}
		return NAN;
	}
	
	static float implK2_GetAttrib(FSkeletonKey Owner, E_AttribKey Attrib, bool& bFound)
	{
		bFound = false;
		if(UArtilleryDispatch::SelfPtr)
		{
			AttrPtr AttributePtr = UArtilleryDispatch::SelfPtr->GetAttrib( Owner, Attrib);
			if(AttributePtr != nullptr)
			{
				bFound = true;
				return AttributePtr->GetCurrentValue();
			}
		}
		return NAN;
	}
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetRelatedKey", DisplayName = "Get Related Key From", ExpandBoolAsExecs="bFound"), Category="Artillery|Keys")
	static FSkeletonKey K2_GetIdentity(FSkeletonKey Owner, E_IdentityAttrib Attrib, bool& bFound)
	{
		bFound = false;
		return implK2_GetIdentity(Owner, Attrib, bFound);
	}

	static UArtilleryDispatch* GetArtilleryDispatch_LowSafety()
	{
		return UArtilleryDispatch::SelfPtr;
	}
	
	static FSkeletonKey implK2_GetIdentity(FSkeletonKey Owner, E_IdentityAttrib Attrib, bool& bFound)
	{
		bFound = false;
		if(UArtilleryDispatch::SelfPtr)
		{
			IdentPtr ident = UArtilleryDispatch::SelfPtr->GetIdent( Owner, Attrib);
			if(ident)
			{
				bFound = true;
				return ident->CurrentValue;
			}
		}
		return FSkeletonKey();
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetPlayerRelatedKey", DisplayName = "Get Local Player's Related Key", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", ExpandBoolAsExecs="bFound"),  Category="Artillery|Keys")
	static FSkeletonKey K2_GetPlayerIdentity(UObject* WorldContextObject, E_IdentityAttrib Attrib, bool& bFound)
	{
		bFound = false;
		UCanonicalInputStreamECS* ptr = WorldContextObject->GetWorld()->GetSubsystem<UCanonicalInputStreamECS>();
		if(ptr)
		{
			InputStreamKey streamkey = ptr->GetStreamForPlayer(PlayerKey::CABLE);
			ActorKey key = ptr->ActorByStream(streamkey);
			if(key)
			{
				return  implK2_GetIdentity(key, Attrib, bFound);
			}
		}
		bFound = false;
		return FSkeletonKey();
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetThisActorAttribute", DisplayName = "Get My Actor's Attribute", DefaultToSelf = "Actor", HidePin = "Actor", ExpandBoolAsExecs="bFound"),  Category="Artillery|Attributes")
	static float K2_GetMyAttrib(AActor *Actor, E_AttribKey Attrib, bool& bFound)
	{
		bFound = false;
		UKeyCarry* ptr = Actor->GetComponentByClass<UKeyCarry>();
		if(ptr)
		{
			if(FSkeletonKey key = ptr->GetMyKey())
			{
				return implK2_GetAttrib(key, Attrib, bFound);
			}
		}
		bFound = false;
		return NAN;
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetPlayerVector", DisplayName = "Get Local Player's Vector Attribute", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", ExpandBoolAsExecs="bFound"),  Category="Artillery|Attributes")
	static FVector K2_GetPlayerVector(UObject* WorldContextObject, E_VectorAttrib Attrib, bool& bFound)
	{
		return K2_GetAnyPlayerVector(WorldContextObject, Attrib, E_PlayerKEY::CABLE, bFound);
	}

	static bool GetAnyPlayerVector(UWorld* ZWorld, E_VectorAttrib Attrib, E_PlayerKEY Player, bool& bFound, FVector& Value)
	{
		if (ZWorld)
		{
			UCanonicalInputStreamECS* ptr = ZWorld->GetSubsystem<UCanonicalInputStreamECS>();
			if(ptr)
			{
				InputStreamKey streamkey = ptr->GetStreamForPlayer(Player);
				ActorKey key = ptr->ActorByStream(streamkey);
				if(key)
				{
					Attr3Ptr attr3p = implK2_GetAttr3Ptr(key, Attrib, bFound);
					if(attr3p)
					{
						Value = attr3p->CurrentValue;
						return true;
					}
				}
			}
		}
		bFound = false;
		return false;
	}

	static bool GetAnyPlayerAttrib(UWorld* ZWorld, E_AttribKey Attrib, E_PlayerKEY Player, bool& bFound, float& Value)
	{
		if (ZWorld)
		{
			UCanonicalInputStreamECS* ptr = ZWorld->GetSubsystem<UCanonicalInputStreamECS>();
			if(ptr)
			{
				InputStreamKey streamkey = ptr->GetStreamForPlayer(Player);
				ActorKey key = ptr->ActorByStream(streamkey);
				if(key)
				{
					Value = implK2_GetAttrib(key, Attrib, bFound);
					return true;
					}
				}
			}
		bFound = false;
		Value = NAN;
		return false;
	}
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetAnyPlayerVector", DisplayName = "Get Any Player's Vector Attribute", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", ExpandBoolAsExecs="bFound"),  Category="Artillery|Attributes")
	static FVector K2_GetAnyPlayerVector(UObject* WorldContextObject, E_VectorAttrib Attrib, E_PlayerKEY Player, bool& bFound)
	{
		bFound = false;
		FVector Value;
		return GetAnyPlayerVector(WorldContextObject->GetWorld(), Attrib, Player, bFound, Value) ? Value : FVector();
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "GetPlayerAttribute", DisplayName = "Get Local Player's Attribute", WorldContext = "WorldContextObject", HidePin = "WorldContextObject"),  Category="Artillery|Attributes")
	static float K2_GetPlayerAttrib(UObject* WorldContextObject, E_AttribKey Attrib)
	{
		UCanonicalInputStreamECS* ptr = WorldContextObject->GetWorld()->GetSubsystem<UCanonicalInputStreamECS>();
		if(ptr)
		{
			InputStreamKey streamkey = ptr->GetStreamForPlayer(PlayerKey::CABLE);
			ActorKey key = ptr->ActorByStream(streamkey);
			if(key)
			{
				return implK2_GetAttrib(key, Attrib);
			}
		}
		return NAN;
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "ApplyDamage", DisplayName = "Apply Damage", ExpandBoolAsExecs="bSuccess"),  Category="Artillery|Attributes")
	static void K2_ApplyDamage(FSkeletonKey Target, float DamageToApply, bool& bSuccess)
	{
		bSuccess = ApplyDamage(Target, DamageToApply);
	}

	static bool ApplyDamage(FSkeletonKey Target, float DamageToApply)
	{
		bool bSuccess = false;
		if(UArtilleryDispatch::SelfPtr)
		{
			if(AttrPtr TargetProposedDamageAtr = UArtilleryDispatch::SelfPtr->GetAttrib(Target, E_AttribKey::ProposedDamage))
			{
				bSuccess = true;
				TargetProposedDamageAtr->AddToCurrentValue(DamageToApply);
			}
		}
		return bSuccess;
	}

	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetGameplayTagsByKey", DisplayName = "Get Gameplay Tags for Key", ExpandBoolAsExecs="bFound"),  Category="Artillery|Tags")
	static UArtilleryGameplayTagContainer* K2_GetTagsByKey(FSkeletonKey Key, bool& bFound)
	{
		FConservedTags Tags = UArtilleryDispatch::SelfPtr->GetExistingConservedTags(Key);
		if (Tags.IsValid())
		{
			bFound = true;
			UArtilleryGameplayTagContainer* TagRef = NewObject<UArtilleryGameplayTagContainer>();
			TagRef->Initialize( Key, UArtilleryDispatch::SelfPtr, true);
			return TagRef;
		}
		bFound = false;
		return nullptr;
	}
	
	static FConservedTags InternalTagsByKey(FSkeletonKey Key, bool& bFound)
	{
		FConservedTags ret = UArtilleryDispatch::SelfPtr->GetOrRegisterConservedTags(Key);
		bFound = ret.IsValid();
		return ret;
	}
	
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "GetPlayerGameplayTags", DisplayName = "Get Local Player's Gameplay Tags", WorldContext = "WorldContextObject", HidePin = "WorldContextObject", ExpandBoolAsExecs="bFound"),  Category="Artillery|Tags")
	static UArtilleryGameplayTagContainer* K2_GetPlayerTags(UObject* WorldContextObject, bool& bFound)
	{
		bFound = false;
		UCanonicalInputStreamECS* ptr = WorldContextObject->GetWorld()->GetSubsystem<UCanonicalInputStreamECS>();
		if(ptr)
		{
			InputStreamKey streamkey = ptr->GetStreamForPlayer(PlayerKey::CABLE);
			ActorKey key = ptr->ActorByStream(streamkey);
			if(key)
			{
				return K2_GetTagsByKey(key, bFound);
			}
		}
		bFound = false;
		return nullptr;
	}
	
	//DEPRECATED
	//TODO: This needs to be replaced by GetPlayerBarrageAgent(PlayerKey)
	static TObjectPtr<UBarragePlayerAgent> GetLocalPlayerBarrageAgent()
	{
		if (UTransformDispatch::SelfPtr && UCanonicalInputStreamECS::SelfPtr)
		{
			InputStreamKey local = UCanonicalInputStreamECS::SelfPtr->GetStreamForPlayer(PlayerKey::CABLE);
			if (local != 0)
			{
				ActorKey playerkey = UCanonicalInputStreamECS::SelfPtr->ActorByStream(local);
				AActor* SecretName = UTransformDispatch::SelfPtr->GetAActorByObjectKey(playerkey).Get();
				if (SecretName && SecretName->GetComponentByClass<UBarragePlayerAgent>()->IsReady)
				{
					return SecretName->GetComponentByClass<UBarragePlayerAgent>();
				}
			}
		}
		return nullptr;
	}
	
	static FSkeletonKey GetLocalPlayerKey_LOW_SAFETY()
	{
		if (UTransformDispatch::SelfPtr && UCanonicalInputStreamECS::SelfPtr)
		{
			InputStreamKey local = UCanonicalInputStreamECS::SelfPtr->GetStreamForPlayer(PlayerKey::CABLE);
			if (local != 0)
			{
				return UCanonicalInputStreamECS::SelfPtr->ActorByStream(local);
			}
		}
		return FSkeletonKey();
	}

	//we need a better and less dangerous idiom than this, you were right, @Maslab.
	static AActor* GetLocalPlayer_UNSAFE()
	{
		if (UTransformDispatch::SelfPtr && UCanonicalInputStreamECS::SelfPtr)
		{
			InputStreamKey local = UCanonicalInputStreamECS::SelfPtr->GetStreamForPlayer(PlayerKey::CABLE);
			if (local != 0)
			{
				ActorKey playerkey = UCanonicalInputStreamECS::SelfPtr->ActorByStream(local);
				return UTransformDispatch::SelfPtr->GetAActorByObjectKey(playerkey).Get();
			}
		}
		return nullptr;
	}
	
	//DEPRECATED
	//TODO: This needs to be replaced by GetPlayerVectors(Forward, Right, PlayerKey)
	static void GetLocalPlayerVectors(FVector& Forward, FVector& Right)
	{
		TObjectPtr<UBarragePlayerAgent> local = GetLocalPlayerBarrageAgent();
		if(local && local->IsReady)
		{
			Forward = local->Chaos_LastGameFrameForwardVector();
			Right = local->Chaos_LastGameFrameRightVector();
		}
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "GetPlayerVectors", DisplayName = "Get Local Player's Attribute"),  Category="Artillery|Character")
	static void K2_GetLocalPlayerVectors(FVector& Forward, FVector& Right)
	{
		GetLocalPlayerVectors(Forward, Right);
	}

	static void GetLocalPlayerVelocity(FVector& Velocity)
	{
		TObjectPtr<UBarragePlayerAgent> local = GetLocalPlayerBarrageAgent();
		if(local && local->IsReady)
		{
			Velocity = FVector(local->GetVelocity());
		}
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "GetPlayerVelocity", DisplayName = "Get Local Player's Velocity"),  Category="Artillery|Character")
	static void K2_GetLocalPlayerVelocity(FVector& Velocity)
	{
		GetLocalPlayerVelocity(Velocity);
	}

	static void SimpleEstimator(FVector& Forwardish, double Counter = 15)
	{
		FVector Right;
		GetLocalPlayerVectors(Forwardish, Right);
		TArray<FArtilleryShell> In;
		UInputECSLibrary::GetHistoricalInputs(In, Counter);
		double accumulateX = 0;
		double accumulateY = 0;
		for(FArtilleryShell& shell : In)
		{
			accumulateX += shell.GetStickLeftX();
			accumulateY += shell.GetStickLeftY();
		}
		accumulateX = accumulateX/Counter;
		accumulateY = accumulateY/Counter;
		//for serious work, replace this.
		accumulateX += In[0].GetStickLeftX();
		accumulateX += In[0].GetStickLeftX();
		accumulateX += In[0].GetStickLeftX();
		accumulateY += In[0].GetStickLeftY();
		accumulateY += In[0].GetStickLeftY();
		accumulateY += In[0].GetStickLeftY();
		accumulateX = accumulateX/4.0;
		accumulateY = accumulateY/4.0;
		TObjectPtr<UBarragePlayerAgent> bind = GetLocalPlayerBarrageAgent();
		if(bind)
		{
			FVector moveX = accumulateX * bind->Acceleration * Right;
			FVector moveY = accumulateY * bind->Acceleration * Forwardish;
			Forwardish = moveX + moveY;
		}
	}

	static UFireControlMachine* GetLocalPlayerFirecOntrolMachine()
	{
		IArtilleryFireControlInterface* FireControlInterface = Cast<IArtilleryFireControlInterface>(GetLocalPlayer_UNSAFE());
		return FireControlInterface != nullptr ? FireControlInterface->GetFireControlMachine() : nullptr;
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "GetPlayerDirectionEstimator", DisplayName = "Get Local Player's Direction Estimator"),  Category="Artillery|Character")
	static void K2_GetPlayerDirectionEstimator(FVector& Forward)
	{
		 SimpleEstimator(Forward, 15);
	}

	//differs from tombstone primitive ONLY in that it will NOT kill non-projectiles.
	static bool SafelyDeleteProjectile(FSkeletonKey Target)
	{
		if(UArtilleryDispatch::SelfPtr && UArtilleryProjectileDispatch::SelfPtr && UArtilleryProjectileDispatch::SelfPtr->IsArtilleryProjectile(Target))
		{
			TombstonePrimitive(Target);
			return true;
		}
		return false;
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "DeleteArtilleryProjectile", DisplayName = "Kills a projectile without asking enough questions. True if found."),  Category="Artillery|Physics")
	static bool K2_DeleteProjectile(FSkeletonKey Target)
	{
		return SafelyDeleteProjectile(Target);
	}

	static bool TombstonePrimitive(FSkeletonKey Target)
	{
		// If a projectile, handle tombstone/delete differently through the projectile manager.
		if(UArtilleryDispatch::SelfPtr && UBarrageDispatch::SelfPtr)
		{
			ArtilleryTime Now = UArtilleryDispatch::SelfPtr->GetShadowNow();
			FBLet Prim = UArtilleryDispatch::SelfPtr->GetFBLetByObjectKey(Target, Now);
			if (Prim && Prim->Me == FBShape::Projectile)
			{
				UArtilleryProjectileDispatch::SelfPtr->DeleteProjectile(Target); // quite a bit extra has to happen, but it does all happen.
			}
			return UBarrageDispatch::SelfPtr->SuggestTombstone(Prim) != 1;
		}
		return false;
	}

	UFUNCTION(BlueprintPure, meta = (ScriptName = "TombstoneAPrimitive", DisplayName = "Kills a primitive without asking enough questions. True if found."),  Category="Artillery|Physics")
	static bool K2_TombstonePrimitive(FSkeletonKey Target)
	{
		return TombstonePrimitive(Target);
	}

	/**
	 * Spawn and Dispatch a timer ticklite on the current script owner.
	 * @return True if ticklite was successfully dispatched
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptName = "SpawnAndDispatchTimerTicklite", DisplayName = "SpawnAndDispatchTimerTicklite", DefaultToSelf = "Owner"), Category = "Artillery|Ticklites")
	static UTimerTickliteHandlerComponent* K2_SpawnAndDispatchTimerTickliteOnObject(AActor* Owner, int LifetimeInTicks)
	{
		UActorComponent* NewTimerTickliteComponent = Owner->AddComponentByClass(UTimerTickliteHandlerComponent::StaticClass(), false, Owner->GetTransform(), false);
		if (NewTimerTickliteComponent == nullptr)
		{
			return nullptr;
		}

		UTimerTickliteHandlerComponent* TimerComponent = Cast<UTimerTickliteHandlerComponent>(NewTimerTickliteComponent);
		FTTimer TimerTicklite(TimerComponent, LifetimeInTicks);
		UArtilleryDispatch::SelfPtr->RequestAddTicklite(MakeShareable(new TL_Timer(TimerTicklite)), Early);
		return TimerComponent;
	}

	UFUNCTION(BlueprintPure, Category = "SkeletonTypes")
	static void BreakSkeletonKey(const FSkeletonKey& Key, int& KeyValue)
	{
		KeyValue = Key.Obj;
	}
};
