#include "ThistleDispatch.h"

#include "ArtilleryBPLibs.h"
#include "ArtillerySkeletalMeshDispatch.h"
#include "Anim/MegafunkUtilsAnimAccessors.h"
#include "ThistleBehavioralist.h"

bool UThistleDispatch::RegistrationImplementation()
{
	return true;
}

void UThistleDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	ThistleBehavioralist = Collection.InitializeDependency<UThistleBehavioralist>();
	SkeletalMeshDispatch = Collection.InitializeDependency<UArtillerySkeletalMeshDispatch>();
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UThistleDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
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
	if (TicksSoFar % 32 == 0 && ensure(ThistleBehavioralist))
	{
		QuadTreeMaintenance = true;
		TSharedPtr<TQuadTree<TPair<ActorKey, FVector2d>>> HoldOpen = QuadTreeForDistance; // retain the ref to the old map until our tick is finished.
		TSharedPtr<TQuadTree<TPair<ActorKey, FVector2d>>> QuadTreeCandidate = MakeShareable(new TQuadTree<TPair<ActorKey, FVector2d>>(FBox2d(FVector2d::ZeroVector - 200000, FVector2d::ZeroVector + 200000)));  //swap now.
		for(TTuple<ActorKey, TObjectPtr<AThistleInject>>& Enemy : ThistleBehavioralist->ActorToThistleAIMapping)
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

bool UThistleDispatch::RegisterNewActorAnimState(const ActorKey NewKey, AActor* Actor) {
	if (!Actor->IsValidLowLevelFast()) {
		return false;
	}
	
	
	USkeletalMeshComponent* SkeletalMeshComp = Actor->GetComponentByClass<USkeletalMeshComponent>();
	
	if (!SkeletalMeshComp) {
		return false;
	}
	
	if (!SkeletalMeshComp->AnimClass) {
		return false;
	}

	USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMeshAsset();
	
	// @todo fighting over this required bones pointer might be unsafe! We may have to copy this out soon
	TSharedPtr<struct FBoneContainer> RequiredBones = SkeletalMeshComp->GetSharedRequiredBones();
	if (ensure(SkeletalMesh) && ensure(RequiredBones)) {

		SkeletalMeshDispatch->CreateAnimInstanceStateForUpdate(NewKey,
																	SkeletalMeshComp->AnimClass,
		                                                         *SkeletalMesh,
		                                                         RequiredBones.ToSharedRef(),
		                                                         SkeletalMeshComp->FillComponentSpaceTransformsRequiredBones,
		                                                         SkeletalMeshComp->GetCurveFilterSettings(),
		                                                         SkeletalMeshComp->GetRefPoseOverride());
		
	}
	return true;
}

void UThistleDispatch::UnregisterActorAnimState(const ActorKey ActorKey) {
	// ActorToAIAnimStateMapping.Remove(ActorKey);
	
	SkeletalMeshDispatch->UnregisterAnimInstanceStateForUpdate(ActorKey);
}

void UThistleDispatch::Tick(float DeltaTime)
{
}

TStatId UThistleDispatch::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UThistleDispatch, STATGROUP_Tickables);
}
