#include "ThistleDispatch.h"

#include "ArtilleryBPLibs.h"
#include "ThistleBehavioralist.h"

bool UThistleDispatch::RegistrationImplementation()
{
	ActorToAILocomotionMapping = UThistleBehavioralist::SelfPtr->ActorToThistleAIMapping;
	return true;
}

void UThistleDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UThistleDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	if ([[maybe_unused]] const UWorld* World = InWorld.GetWorld()) {
		UE_LOG(LogTemp, Warning, TEXT("ThistleDispatch:Subsystem: World beginning play"));
	}
}

void UThistleDispatch::Deinitialize()
{
	Super::Deinitialize();
}

void UThistleDispatch::ArtilleryTick(uint64_t TicksSoFar)
{
	//build distance map
	if (TicksSoFar % 32 && ActorToAILocomotionMapping)
	{
		QuadTreeMaintenance = true;
		TSharedPtr<TQuadTree<TPair<ActorKey, FVector2d>>> HoldOpen = QuadTreeForDistance; // retain the ref to the old map until our tick is finished.
		TSharedPtr<TQuadTree<TPair<ActorKey, FVector2d>>> QuadTreeCandidate = MakeShareable(new TQuadTree<TPair<ActorKey, FVector2d>>(FBox2d(FVector2d::ZeroVector - 200000, FVector2d::ZeroVector + 200000)));  //swap now.
		for(TTuple<ActorKey, TObjectPtr<AThistleInject>>& Enemy : *ActorToAILocomotionMapping)
		{
			bool YouAliveInThere = false;
			FVector center = UArtilleryLibrary::implK2_GetLocation(Enemy.Key, YouAliveInThere);
			if (YouAliveInThere)
			{
				FVector2d TwoDCenter = FVector2d(center.X, center.Y);
				FBox2d Box(TwoDCenter - 100, TwoDCenter +100);
				QuadTreeCandidate->Insert( TPair<ActorKey, FVector2d>(Enemy.Key, TwoDCenter), Box);
			}
		}
		QuadTreeForDistance = QuadTreeCandidate;
		QuadTreeMaintenance = false;
	}
}

void UThistleDispatch::Tick(float DeltaTime)
{
}

TStatId UThistleDispatch::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UThistleDispatch, STATGROUP_Tickables);
}
