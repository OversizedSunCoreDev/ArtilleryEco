// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include "ArtilleryDispatch.h"
#include "ThistleBehavioralist.h"

struct TargetGroupingInfo
{
	uint32 TargetsInImpactRadiusCount;
	FVector TargetLocation;
};

class FTSpreadFire : public UArtilleryDispatch::TL_ThreadedImpl
{
	uint32 TicksRemaining;
	FBarrageKey SourceObject;

	// Firing data
	uint32 NumberOfGroupsToFind;
	double ImpactRadius;

	// Search data
	uint32 NumberOfActors;
	ActorKeyArray* ActorsToSearch;
	
	TSet<uint32> EnemyBodyIDs;
	TMap<ActorKey, TargetGroupingInfo> BodyIDToGroupingInfo;

	std::function<void(FVector)> LaunchProjectileCallback;

public:
	FTSpreadFire(FBarrageKey OwnerObject, uint32 CountOfGroups, uint32 ImpactRadiusMeters, uint32 ActorCount, ActorKeyArray* ActorsArray, const std::function<void(FVector)> CallbackFunc)
	{
		TicksRemaining = 4;
		SourceObject = OwnerObject;
		NumberOfGroupsToFind = CountOfGroups;
		ImpactRadius = ImpactRadiusMeters;
		NumberOfActors = ActorCount;
		ActorsToSearch = ActorsArray;
		
		EnemyBodyIDs.Reserve(MAX_ENEMY_COUNT);
		BodyIDToGroupingInfo.Reserve(MAX_ENEMY_COUNT);
		LaunchProjectileCallback = CallbackFunc;
	}
	
	FTSpreadFire() : TicksRemaining(2), SourceObject(0), NumberOfGroupsToFind(0), ImpactRadius(0), NumberOfActors(0)
	{
		ActorsToSearch = nullptr;
		EnemyBodyIDs.Reserve(MAX_ENEMY_COUNT);
		BodyIDToGroupingInfo.Reserve(MAX_ENEMY_COUNT);
		LaunchProjectileCallback = nullptr;
	}
	
	void TICKLITE_StateReset()
	{
	}

	void TICKLITE_Calculate()
	{
		UBarrageDispatch* Physics = this->ADispatch->DispatchOwner->GetWorld()->GetSubsystem<UBarrageDispatch>();
		check(Physics);
		UTransformDispatch* TransformDispatch = this->ADispatch->DispatchOwner->GetWorld()->GetSubsystem<UTransformDispatch>();
		check(TransformDispatch);

		// Query potential targets
		TArray<uint32> BodyIDsFound;
		BodyIDsFound.Reserve(MAX_FOUND_OBJECTS);
		for (uint32 ActorIndex = 0; ActorIndex < NumberOfActors; ++ActorIndex)
		{
			const ActorKey& CurrentKey = (*ActorsToSearch)[ActorIndex];
			auto WeakPtrToActor =TransformDispatch->GetAActorByObjectKey(CurrentKey);
			auto ActorPin = WeakPtrToActor.Pin();
			if (ActorPin)
			{
				auto ActorLocation = ActorPin->GetActorLocation();
				FBLet ActorFiblet = this->ADispatch->GetFBLetByObjectKey(CurrentKey, this->ADispatch->GetShadowNow());
				if (ActorFiblet == nullptr)
				{
					continue;
				}
				const JPH::DefaultBroadPhaseLayerFilter BroadPhaseFilter = Physics->GetDefaultBroadPhaseLayerFilter(Layers::CAST_QUERY);
				const auto ObjectLayerFilter = Physics->GetDefaultLayerFilter(Layers::CAST_QUERY);
				const JPH::IgnoreSingleBodyFilter BodyFilter = Physics->GetFilterToIgnoreSingleBody(ActorFiblet);
			
				uint32 BodiesFoundNearTarget = 0;
				Physics->SphereSearch(SourceObject, ActorLocation, this->ImpactRadius, BroadPhaseFilter, ObjectLayerFilter, BodyFilter, &BodiesFoundNearTarget, BodyIDsFound);

				// Process bodies we found to count enemies
				uint32 EnemyCounter = 0;
				for (uint32 BodyCounterIndex = 0; BodyCounterIndex < BodiesFoundNearTarget; ++BodyCounterIndex)
				{
					const uint32 BodyID = BodyIDsFound[BodyCounterIndex];

					// If the other body isn't in the map, we need to check if it's an enemy
					if (!EnemyBodyIDs.Contains(BodyID))
					{
						bool IsEnemy = true;
						FBarrageKey BodyBarrageKey = Physics->GenerateBarrageKeyFromBodyId(BodyID);
						FBLet BodyObjectFiblet = Physics->GetShapeRef(BodyBarrageKey);
						if (BodyObjectFiblet)
						{
							FSkeletonKey BodyObjectKey = BodyObjectFiblet->KeyOutOfBarrage;
					
							if (ADispatch->DispatchOwner->DoesEntityHaveTag(BodyObjectKey, FGameplayTag::RequestGameplayTag("Enemy")))
							{
								EnemyBodyIDs.Add(BodyID);
							}
							else
							{
								IsEnemy = false;
							}
						}

						EnemyCounter += IsEnemy;
					}
				}

				TargetGroupingInfo& NewGroupingInfo = BodyIDToGroupingInfo.Add(CurrentKey, TargetGroupingInfo{});
				NewGroupingInfo.TargetLocation = ActorLocation;
				NewGroupingInfo.TargetsInImpactRadiusCount = EnemyCounter;
			}

			// Sort is unstable, so may lead to unexpected but likely unnoticed behavior
			// It is the easiest way to get the arbitrarily top X number of results, but there may be other
			// heuristics we'll want to add later
			BodyIDToGroupingInfo.ValueSort([](const TargetGroupingInfo& Left, const TargetGroupingInfo& Right)
			{
				return Left.TargetsInImpactRadiusCount > Right.TargetsInImpactRadiusCount;
			});
		}
	}

	void TICKLITE_Apply()
	{
		--TicksRemaining;
		if (LaunchProjectileCallback)
		{
			uint32 TargetsProcessed = 0;
			for (auto it = BodyIDToGroupingInfo.CreateConstIterator(); it && TargetsProcessed < NumberOfGroupsToFind; ++it, ++TargetsProcessed)
			{
				LaunchProjectileCallback(it.Value().TargetLocation);
			}

			TicksRemaining = 0;
		}
	}

	void TICKLITE_CoreReset()
	{
	}

	bool TICKLITE_CheckForExpiration()
	{
		return TicksRemaining == 0;
	}

	void TICKLITE_OnExpiration()
	{
	}
};

using TL_SpreadFire = Ticklites::Ticklite<FTSpreadFire>;
