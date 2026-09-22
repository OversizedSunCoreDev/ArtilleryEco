// Copyright 2026 Oversized Sun Inc. All Rights Reserved.

#include "ArtilleryBPLibs.h"

#include "ArtilleryGunBlueprint.h"
#include "ArtilleryRuntimeModule.h"
#include "InventoryDispatch.h"
#include "PhysicsTypes/BarragePlayerAgent.h"
#include "UArtilleryGameplayTagContainer.h"
#ifdef JPH_DEBUG_RENDERER
#include "Debug/BarrageDebugDraw.h"
#endif

bool UArtilleryReadOnlyAccessLibrary::ReadAttribute(const UObject* WorldContextObject, FSkeletonKey Owner, E_AttribKey Attrib, float& OutAttribute)
{
	return UArtilleryLibrary::GetAttribute(UArtilleryDispatch::Get(WorldContextObject->GetWorld()), Owner, Attrib, OutAttribute);
}

bool UArtilleryReadOnlyAccessLibrary::ReadLocalPlayerAttribute(const UObject* WorldContextObject, E_AttribKey Attrib, float& OutAttribute)
{
	return UArtilleryLibrary::GetAttribute(UArtilleryDispatch::Get(WorldContextObject->GetWorld()), GetLocalPlayerKey(WorldContextObject), Attrib, OutAttribute);
}

bool UArtilleryReadOnlyAccessLibrary::ReadLocalPlayerVectorAttribute(const UObject* WorldContextObject, E_VectorAttrib VectorAttrib, FVector& OutAttribute)
{
	return UArtilleryLibrary::GetPlayerVector(UArtilleryDispatch::Get(WorldContextObject->GetWorld()), VectorAttrib, E_PlayerKEY::CABLE, OutAttribute);
}

bool UArtilleryReadOnlyAccessLibrary::ReadLocalPlayerIdentity(const UObject* WorldContextObject, E_IdentityAttrib IdentityAttrib, FSkeletonKey& OutIdentity)
{
	bool bFound = false;
	OutIdentity = UArtilleryLibrary::GetIdentity(UArtilleryDispatch::Get(WorldContextObject->GetWorld()), GetLocalPlayerKey(WorldContextObject), IdentityAttrib, bFound);
	return bFound;
}

FSkeletonKey UArtilleryReadOnlyAccessLibrary::GetLocalPlayerKey(const UObject* WorldContextObject)
{
	const UArtilleryDispatch* Artillery = UArtilleryDispatch::Get(WorldContextObject->GetWorld());
	if (Artillery && Artillery->InputStreamECS)
	{
		UCanonicalInputStreamECS* InputECS = Artillery->InputStreamECS;
		InputStreamKey local = InputECS->GetStreamForPlayer(PlayerKey::CABLE);
		if (local != 0)
		{
			return InputECS->ActorByStream(local);
		}
	}

	return FSkeletonKey();
}

void UInputECSLibrary::GetHistoricalInputs(UCanonicalInputStreamECS* InputECS, TArray<FArtilleryShell>& Inputs, int Count)
{
	if (InputECS)
	{
		InputStreamKey streamkey = InputECS->GetStreamForPlayer(PlayerKey::CABLE);
		TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> sptr = InputECS->GetStream(streamkey);
		for (int InputIndex = 0; InputIndex <= Count; ++InputIndex)
		{
			std::optional<FArtilleryShell> input = sptr.Get()->peek(sptr->GetHighestGuaranteedInput() - InputIndex);
			Inputs.Add(input.has_value() ? input.value() : FArtilleryShell());
		}
	}
}

void UInputECSLibrary::K2_Get15LocalHistoricalInputs(UCanonicalInputStreamECS* InputECS, TArray<FArtilleryShell>& Inputs)
{
	GetHistoricalInputs(InputECS, Inputs, 15);
}

EArtilleryFindResult UArtilleryLibrary::K2_GetLocation(FSkeletonKey Owner, FVector& OutLocation)
{
	bool bFound = false;
	OutLocation = GetLocation(FArtilleryDispatchThreadScope::ArtilleryDispatch, Owner,bFound);
	if (bFound)
	{
		return EArtilleryFindResult::Found;
	}
	
	return EArtilleryFindResult::NotFound;
}

EArtilleryFindResult UArtilleryLibrary::K2_GetAttribute(FSkeletonKey Owner, E_AttribKey Attrib, float& OutAttribute)
{
	if (GetAttribute(FArtilleryDispatchThreadScope::ArtilleryDispatch, Owner, Attrib, OutAttribute))
	{
		return EArtilleryFindResult::Found;
	}
	
	return EArtilleryFindResult::NotFound;

}

EArtilleryFindResult UArtilleryLibrary::K2_GetVectorAttribute(FSkeletonKey Owner, E_VectorAttrib Attrib, FVector& OutVectorAttribute)
{
	if (FArtilleryDispatchThreadScope::ArtilleryDispatch)
	{
		Attr3Ptr Attribute = FArtilleryDispatchThreadScope::ArtilleryDispatch->GetVecAttr(Owner, Attrib);
		
		if (Attribute)
		{
			OutVectorAttribute = Attribute->CurrentValue;
			return EArtilleryFindResult::Found;
		}
	}

	OutVectorAttribute = FVector(NAN);
	return EArtilleryFindResult::NotFound;
}

EArtilleryFindResult UArtilleryLibrary::K2_GetIdentity(FSkeletonKey Owner, E_IdentityAttrib Attrib, FSkeletonKey& OutIdentity)
{
	bool bFound = false;
	OutIdentity = GetIdentity(FArtilleryDispatchThreadScope::ArtilleryDispatch, Owner, Attrib, bFound);
	if (bFound)
	{
		return EArtilleryFindResult::Found;
	}

	return  EArtilleryFindResult::NotFound;
}

EArtilleryFindResult UArtilleryLibrary::K2_PlaySoundEffectAt(const FSoundPinBag& SoundName, FSkeletonKey Source, const FVector& Location)
{
	if (PlaySoundEffectAt(FArtilleryDispatchThreadScope::ArtilleryDispatch, SoundName, Source, Location))
	{
		return EArtilleryFindResult::Found;
	}

	return EArtilleryFindResult::NotFound;
}

void UArtilleryLibrary::K2_DrawDebugLine(const FVector& Start, const FVector& End, FColor Color, float Time)
{
#ifdef JPH_DEBUG_RENDERER
	//@todo for thread safety this should be per-thread
	if (JPH::DebugRenderer::sInstance)
	{
		JPH::DebugRenderer::sInstance->DrawLine(CoordinateUtils::ToJoltCoordinates(Start),
		                                        CoordinateUtils::ToJoltCoordinates(End),
		                                        JPH::ToJoltColor(Color));
	}
#endif
}

void UArtilleryLibrary::K2_DrawDebugSphere(const FVector& Location, FColor Color, float Radius, float Time)
{
#ifdef JPH_DEBUG_RENDERER
	//@todo for thread safety this should be per-thread
	if (JPH::DebugRenderer::sInstance)
	{
		// FArtilleryDispatchThreadScope::BarrageDispatch->JoltGameSim
		JPH::DebugRenderer::sInstance->DrawSphere(CoordinateUtils::ToJoltCoordinates(Location),
		                                          CoordinateUtils::RadiusToJolt(Radius),
		                                          JPH::ToJoltColor(Color));
	}
#endif
}

void UArtilleryLibrary::K2_DrawDebugString(const FVector& Location, const FString& String, FColor Color)
{
#ifdef JPH_DEBUG_RENDERER
	//@todo for thread safety this should be per-thread
	if (JPH::DebugRenderer::sInstance)
	{
		JPH::DebugRenderer::sInstance->DrawText3D(CoordinateUtils::ToJoltCoordinates(Location),
		                                          JPH::StringFormat("%s", *String),
		                                          JPH::ToJoltColor(Color));
	}
#endif
}

void UArtilleryLibrary::K2_PrintDebugString(const FString& String, bool bOnScreen, bool bLog, float Time, FColor Color, int32 Key)
{
	if (bLog)
	{
		UE_LOG(LogArtillery, Log, TEXT("%s"), *String);
	}

	if (bOnScreen)
	{
		GEngine->AddOnScreenDebugMessage(Key, Time, Color, String);

		// No asynctask needed since this has its own critical section? neat
		// AsyncTask(ENamedThreads::GameThread,
		//           [=]()
		//           {
		//           });
	}
}

EArtilleryFindResult UArtilleryLibrary::K2_GetPlayerVector(UArtilleryDispatch* Dispatch,
                                                           E_VectorAttrib Attrib,
                                                           E_PlayerKEY Player,
                                                           FVector& OutVector)
{
	if (GetPlayerVector(FArtilleryDispatchThreadScope::ArtilleryDispatch, Attrib, Player, OutVector))
	{
		return EArtilleryFindResult::Found;
	}

	return EArtilleryFindResult::NotFound;
}

int32 UArtilleryLibrary::GetTotalsTickCount(UArtilleryDispatch* MyDispatch)
{
	return MyDispatch ? MyDispatch->ArtilleryAsyncWorldSim.SeqNumber / 4 : 0;
}

FVector UArtilleryLibrary::GetLocation(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, bool& bFound)
{
	bFound = false;
	if (Dispatch)
	{
		//welp. more like itty bitty living space actually.
		FBLet Fiblet = Dispatch->GetFBLetByObjectKey(Owner, Dispatch->GetShadowNow());
		if (Fiblet)
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
	return FVector(NAN); // YOU SHOULD NOT HAVE COME HERE
}

Attr3Ptr UArtilleryLibrary::GetAttr3Ptr(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, E_VectorAttrib Attrib)
{
	if (Dispatch)
	{
		Attr3Ptr AttribVecPtr = Dispatch->GetVecAttr(Owner, Attrib);
		if (AttribVecPtr != nullptr)
		{
			return AttribVecPtr;
		}
	}
	return nullptr;
}

void UArtilleryLibrary::RequestUnboundGun(UArtilleryDispatch* Dispatch,
                                          FARelatedBy Relationship,
                                          const FSkeletonKey& Requester,
                                          const FGunKey& GunKey)
{
	if (Dispatch)
	{
		TSharedPtr<FRequestRouter> RouterHold = Dispatch->RequestRouter;
		if (RouterHold)
		{
			RouterHold->NewUnboundGun(Requester, GunKey, Relationship, Dispatch->GetShadowNow());
		}
	}
}

void UArtilleryLibrary::RequestGunFire(UArtilleryDispatch* Dispatch, const FGunKey& GunKey)
{
	if (Dispatch)
	{
		TSharedPtr<FRequestRouter> RouterHold = Dispatch->RequestRouter;
		if (RouterHold)
		{
			RouterHold->GunFired(GunKey, Dispatch->GetShadowNow());
		}
	}
}

FVector UArtilleryLibrary::GetPlayerLocationAsEstTarget(UArtilleryDispatch* Dispatch, E_PlayerKEY Player)
{
	if (Dispatch)
	{
		FVector Value = FVector::ZeroVector;
		bool bFound = GetPlayerVector(Dispatch, E_VectorAttrib::Location, Player, Value);

		if (bFound && !Value.IsNearlyZero())
		{
			float height = 0.0;
			if (GetPlayerAttribute(Dispatch, E_AttribKey::Height, Player, height))
			{
				FVector Dir = FVector();
				K2_GetPlayerDirectionEstimator(Dispatch, Dir);
				return Value + Dir + (FVector::UpVector * height);
			}
		}
	}
	return FVector();
}

bool UArtilleryLibrary::GetAttribute(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, E_AttribKey AttributeKey, float& OutAttribute)
{
	if (Dispatch)
	{
		AttrPtr Attribute = Dispatch->GetAttrib(Owner, AttributeKey);
		if (Attribute.IsValid())
		{
			OutAttribute = Attribute->GetCurrentValue();
			return true;
		}
	}
	
	OutAttribute = NAN;
	return false;
}

FSkeletonKey UArtilleryLibrary::GetIdentity(UArtilleryDispatch* Dispatch, FSkeletonKey Owner, E_IdentityAttrib Attrib, bool& bFound)
{
	bFound = false;
	if (Dispatch)
	{
		IdentPtr ident = Dispatch->GetIdent(Owner, Attrib);
		if (ident)
		{
			bFound = true;
			return ident->CurrentValue;
		}
	}
	return FSkeletonKey();
}

bool UArtilleryLibrary::PlaySoundEffectAt(UArtilleryDispatch* Dispatch, const FSoundPinBag& SoundName, FSkeletonKey Source, const FVector& Location)
{
	if (UWorld* World = Dispatch ? Dispatch->GetWorld() : nullptr)
	{
		if (SoundName.AvailableSoundsAtEditTime != NAME_None)
		{
			UInventoryDispatch* InventoryDispatch = World->GetSubsystem<UInventoryDispatch>();
			if (ensure(InventoryDispatch))
			{
				FSKEffectTicket key = InventoryDispatch->GetSoundEffectTicket(Source,
				                                                              FSKSoundFXDefinitionKey(SoundName.AvailableSoundsAtEditTime).GetSK(),
				                                                              FEventParameterPackage(Source, 100, Location),
				                                                              false);
				InventoryDispatch->FireSoundEffect(key);
				
				return true;
			}
		}
	}
	
	return false;
}

bool UArtilleryLibrary::GetPlayerVector(UArtilleryDispatch* Dispatch, E_VectorAttrib Attrib, E_PlayerKEY Player, FVector& OutVector)
{
	if (ensure(Dispatch) && ensure(Dispatch->InputStreamECS))
	{
		InputStreamKey streamkey = Dispatch->InputStreamECS->GetStreamForPlayer(Player);
		ActorKey key = Dispatch->InputStreamECS->ActorByStream(streamkey);
		if (key)
		{
			Attr3Ptr attr3p = GetAttr3Ptr(Dispatch, key, Attrib);
			if (attr3p)
			{
				OutVector = attr3p->CurrentValue;
				return true;
			}
		}
	}

	return false;
}

bool UArtilleryLibrary::GetPlayerAttribute(UArtilleryDispatch* Dispatch, E_AttribKey Attrib, E_PlayerKEY Player, float& OutAttribute)
{
	if (Dispatch && Dispatch->InputStreamECS)
	{
		InputStreamKey streamkey = Dispatch->InputStreamECS->GetStreamForPlayer(Player);
		ActorKey key = Dispatch->InputStreamECS->ActorByStream(streamkey);
		if (key)
		{
			return GetAttribute(Dispatch, key, Attrib, OutAttribute);
		}
	}

	OutAttribute = NAN;
	return false;
}


bool UArtilleryLibrary::ApplyDamage(UArtilleryDispatch* Dispatch, const FSkeletonKey Target, float DamageToApply, const FVector& SourceLocation)
{
	bool bSuccess = false;
	if (Dispatch)
	{
		if (AttrPtr TargetProposedDamageAtr = Dispatch->GetAttrib(Target, E_AttribKey::ProposedDamage))
		{
			bSuccess = true;
			TargetProposedDamageAtr->AddToCurrentValue(DamageToApply);
		}
		if (bSuccess && !SourceLocation.IsZero())
		{
			bool bVecFound = false;
			if (Attr3Ptr SourceAttr = GetAttr3Ptr(Dispatch, Target, E_VectorAttrib::TargetLocation))
			{
				SourceAttr->SetCurrentValue(SourceLocation);
			}
		}
	}
	return bSuccess;
}


UArtilleryGameplayTagContainer* UArtilleryLibrary::GetTagsByKey(UArtilleryDispatch* Dispatch, FSkeletonKey Key, bool& bFound)
{
	FConservedTags Tags = Dispatch->GetExistingConservedTags(Key);
	if (Tags.IsValid())
	{
		bFound = true;

		// This makes it safer to newobject on a non-gamethread which artillery can do... We try to avoid this as much as possible but this one is 
		FGCScopeGuard Guard;
		UArtilleryGameplayTagContainer* TagRef = NewObject<UArtilleryGameplayTagContainer>();
		TagRef->Initialize(Key, Dispatch, true);
		return TagRef;
	}
	bFound = false;
	return nullptr;
}

FConservedTags UArtilleryLibrary::InternalTagsByKey(UArtilleryDispatch* Dispatch, FSkeletonKey Key, bool& bFound)
{
	bool Found = false;
	FConservedTags ret = Dispatch->GetOrRegisterConservedTags(Key, Found);
	bFound = ret.IsValid();
	return ret;
}

void UArtilleryLibrary::GetPlayerVectors(UArtilleryDispatch* Dispatch, FVector& Forward, FVector& Right)
{
	UBarragePlayerAgent* PlayerAgent = GetLocalPlayerBarrageAgent(Dispatch);
	if (PlayerAgent && PlayerAgent->IsReady)
	{
		Forward = PlayerAgent->Chaos_LastGameFrameForwardVector();
		Right = PlayerAgent->Chaos_LastGameFrameRightVector();
	}
}

void UArtilleryLibrary::SimpleEstimator(UCanonicalInputStreamECS* ptr, FVector& Forwardish, double Counter)
{
	FVector Right;
	auto Dispatch = ptr->GetWorld()->GetSubsystem<UArtilleryDispatch>();
	GetPlayerVectors(Dispatch, Forwardish, Right);
	TArray<FArtilleryShell> In;
	UInputECSLibrary::GetHistoricalInputs(ptr, In, Counter);
	double accumulateX = 0;
	double accumulateY = 0;
	for (FArtilleryShell& shell : In)
	{
		accumulateX += shell.GetStickLeftX();
		accumulateY += shell.GetStickLeftY();
	}
	accumulateX = accumulateX / Counter;
	accumulateY = accumulateY / Counter;
	//for serious work, replace this.
	accumulateX += In[0].GetStickLeftX();
	accumulateX += In[0].GetStickLeftX();
	accumulateX += In[0].GetStickLeftX();
	accumulateY += In[0].GetStickLeftY();
	accumulateY += In[0].GetStickLeftY();
	accumulateY += In[0].GetStickLeftY();
	accumulateX = accumulateX / 4.0;
	accumulateY = accumulateY / 4.0;
	UBarragePlayerAgent* bind = GetLocalPlayerBarrageAgent(Dispatch);
	if (bind)
	{
		FVector moveX = accumulateX * bind->Acceleration * Right;
		FVector moveY = accumulateY * bind->Acceleration * Forwardish;
		Forwardish = moveX + moveY;
	}
}

void UArtilleryLibrary::K2_GetPlayerDirectionEstimator(UObject* WorldContextObject, FVector& Forward)
{
	SimpleEstimator(UCanonicalInputStreamECS::Get(WorldContextObject), Forward, 15);
}

bool UArtilleryLibrary::SafelyDeleteProjectile(UArtilleryDispatch* Dispatch, FSkeletonKey Target)
{
	auto BulletDispatch = Dispatch->GetWorld()->GetSubsystem<UArtilleryProjectileDispatch>();
	if (Dispatch && BulletDispatch && BulletDispatch->IsArtilleryProjectile(Target))
	{
		TombstonePrimitive(Dispatch, Target);
		return true;
	}
	return false;
}

bool UArtilleryLibrary::TombstonePrimitive(UArtilleryDispatch* Dispatch, FSkeletonKey Target)
{
	// If a projectile, handle tombstone/delete differently through the projectile manager.
	auto Barrage = Dispatch->GetWorld()->GetSubsystem<UBarrageDispatch>();
	auto BulletDispatch = Dispatch->GetWorld()->GetSubsystem<UArtilleryProjectileDispatch>();
	if (Dispatch && Barrage)
	{
		ArtilleryTime Now = Dispatch->GetShadowNow();
		FBLet Prim = Dispatch->GetFBLetByObjectKey(Target, Now);
		Dispatch->DeregisterGameplayTags(Target); //release tags if any.
		if (Prim && Prim->Me == FBShape::Projectile && BulletDispatch)
		{
			BulletDispatch->DeleteProjectile(Target); // quite a bit extra has to happen, but it does all happen.
		}
		return Barrage->SuggestTombstone(Prim) != 1;
	}
	return false;
}

void UArtilleryLibrary::BreakSkeletonKey(const FSkeletonKey& Key, int32& KeyValue)
{
	KeyValue = Key.Obj;
}

PlayerKey UArtilleryLibrary::GetPlayerKeyFromSkeletonKey(UCanonicalInputStreamECS* InputECS, const FSkeletonKey& Key)
{
	if (InputECS)
	{
		for (auto& Pair : InputECS->SessionPlayerToStreamMapping)
		{
			ActorKey actor = InputECS->ActorByStream(Pair.Value);
			if (actor == Key)
			{
				return Pair.Key;
			}
		}
	}

	return PlayerKey::CABLE;
}

PlayerKey UArtilleryLibrary::GetLocalPlayerKey(UWorld* World)
{
	return PlayerKey::CABLE;
}

UBarragePlayerAgent* UArtilleryLibrary::GetLocalPlayerBarrageAgent(UArtilleryDispatch* Dispatch)
{
	auto TransformDispatch = UTransformDispatch::Get(Dispatch->GetWorld());
	auto InputECS = UCanonicalInputStreamECS::Get(Dispatch->GetWorld());
	if (TransformDispatch && InputECS)
	{
		InputStreamKey local = InputECS->GetStreamForPlayer(PlayerKey::CABLE);
		if (local != 0)
		{
			ActorKey playerkey = InputECS->ActorByStream(local);
			AActor* SecretName = TransformDispatch->GetAActorByObjectKey(playerkey).Get();
			if (SecretName && SecretName->GetComponentByClass<UBarragePlayerAgent>()->IsReady)
			{
				return SecretName->GetComponentByClass<UBarragePlayerAgent>();
			}
		}
	}
	return nullptr;
}

FSkeletonKey UArtilleryLibrary::GetLocalPlayerKey_LOW_SAFETY(UCanonicalInputStreamECS* InputECS)
{
	if (InputECS)
	{
		InputStreamKey local = InputECS->GetStreamForPlayer(PlayerKey::CABLE);
		if (local != 0)
		{
			return InputECS->ActorByStream(local);
		}
	}

	return FSkeletonKey();
}

AActor* UArtilleryLibrary::GetLocalPlayer_UNSAFE(UArtilleryDispatch* Dispatch)
{
	auto TransformDispatch = UTransformDispatch::Get(Dispatch->GetWorld());
	auto InputECS = UCanonicalInputStreamECS::Get(Dispatch->GetWorld());
	if (TransformDispatch && InputECS)
	{
		InputStreamKey local = InputECS->GetStreamForPlayer(PlayerKey::CABLE);
		if (local != 0)
		{
			ActorKey playerkey = InputECS->ActorByStream(local);
			return TransformDispatch->GetAActorByObjectKey(playerkey).Get();
		}
	}
	return nullptr;
}
