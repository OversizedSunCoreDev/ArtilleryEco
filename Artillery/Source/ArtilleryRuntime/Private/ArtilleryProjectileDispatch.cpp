// Copyright 2024 Oversized Sun Inc. All Rights Reserved.

#include "ArtilleryProjectileDispatch.h"
#include "BarrageContactEvent.h"
#include "ArtilleryBPLibs.h"
#include "BarrageDispatch.h"
#include "FArtilleryGun.h"

void UArtilleryProjectileDispatch::ArtilleryTick()
{
	//On Tick, we see if anybody needs to go.
	++ExpirationCounter;
	auto Ref = Deadliner.UpdateAndConsume();

	for (FSkeletonKey Goner : Ref)
	{
		UArtilleryLibrary::TombstonePrimitive(Goner);
	}
}

bool UArtilleryProjectileDispatch::RegistrationImplementation()
{
	// TODO: Can we find and autoload the datatable, or do
	ProjectileDefinitions = LoadObject<UDataTable>(nullptr, GamePath);
	ProjectileDefinitions = ProjectileDefinitions == nullptr ? LoadObject<UDataTable>(nullptr, EcoPath) : ProjectileDefinitions;
	UE_LOG(LogTemp, Warning, TEXT("ArtilleryProjectileDispatch:Subsystem: Online"));
	ProjectileDefinitions->ForeachRow<FProjectileDefinitionRow>(
		TEXT("UArtilleryProjectileDispatch::PostInitialize"),
		[this](const FName& Key, const FProjectileDefinitionRow& ProjectileDefinition) mutable
		{
			UNiagaraParticleDispatch* NPD = GetWorld()->GetSubsystem<UNiagaraParticleDispatch>();
			check(NPD);
			if (UStaticMesh* StaticMeshPtr = ProjectileDefinition.ProjectileMesh)
			{
				TWeakObjectPtr<AInstancedMeshManager>* MeshManager = MeshAssetToMeshManagerMapping->Find(ProjectileDefinition.ProjectileMesh.GetName());
				if (!MeshManager)
				{
					AInstancedMeshManager* NewMeshManager = GetWorld()->SpawnActor<AInstancedMeshManager>();
					if (NewMeshManager == nullptr)
					{
						UE_LOG(LogTemp, Error,
						       TEXT("Could not spawn mesh manager actor. If this is during editor load, it can be ignored. Otherwise..."));
					}
					NewMeshManager->InitializeManager();
					NewMeshManager->SetStaticMesh(StaticMeshPtr);
					NewMeshManager->SetInternalFlags(EInternalObjectFlags::Async);
					NewMeshManager->SwarmKineManager->SetCanEverAffectNavigation(false);
					NewMeshManager->SwarmKineManager->SetSimulatePhysics(false);
					NewMeshManager->SwarmKineManager->bNavigationRelevant = 0;
					NewMeshManager->SwarmKineManager->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
					NewMeshManager->SwarmKineManager->SetInternalFlags(EInternalObjectFlags::Async);

					FName ProjectileName(ProjectileDefinition.ProjectileDefinitionId);
					ManagerKeyToMeshManagerMapping->Add(NewMeshManager->GetMyKey(), NewMeshManager);
					ProjectileNameToMeshManagerMapping->Add(ProjectileName, NewMeshManager);
					MeshAssetToMeshManagerMapping->Add(*ProjectileDefinition.ProjectileMesh.GetName(), NewMeshManager);
					if (ProjectileDefinition.ParticleEffectDataChannel)
					{
						NPD->AddNDCReference(ProjectileName, ProjectileDefinition.ParticleEffectDataChannel);
					}

					//these are standing in for a pretty messy body of more specific work we could do instead.
					//basically, when you load up a new mesh, there's quite a bit of work and book-keeping that you
					//need to do for instanced static mesh systems because loading a mesh from disk does one thing in editor
					//and something else in non-PIE sessions. There are VERY good reasons for this, but it means you need
					//a lot of specialization. Or you can just NUKE THE SITE FROM ORBIT. which seems to work. and is brainless.
					//and just as performant. welp.
#if WITH_EDITOR
					NewMeshManager->SwarmKineManager->OnMeshRebuild(true);
#endif
					NewMeshManager->SwarmKineManager->ReregisterComponent();
				}
				else
				{
					ProjectileNameToMeshManagerMapping->Add(FName(ProjectileDefinition.ProjectileDefinitionId), *MeshManager);
				}
			}
		});

	MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
	check(MyDispatch);
	UBarrageDispatch* BarrageDispatch = GetWorld()->GetSubsystem<UBarrageDispatch>();
	BarrageDispatch->OnBarrageContactAddedDelegate.AddUObject(this, &UArtilleryProjectileDispatch::OnBarrageContactAdded);
	UArtilleryDispatch::SelfPtr->SetProjectileDispatch(this);
	SelfPtr = this;
	return true;
}

void UArtilleryProjectileDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
	Super::Initialize(Collection);
}

void UArtilleryProjectileDispatch::PostInitialize()
{
	Super::PostInitialize();
}

void UArtilleryProjectileDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UArtilleryProjectileDispatch::Deinitialize()
{
	TSharedPtr<KeyToItemCuckooMap> HoldOpen = ProjectileKeyToMeshManagerMapping;
	TSharedPtr<TMap<FString, TWeakObjectPtr<AInstancedMeshManager>>> HoldOpenManagers = MeshAssetToMeshManagerMapping;
	ManagerKeyToMeshManagerMapping->Empty();
	ProjectileNameToMeshManagerMapping->Empty();
	ProjectileToGunMapping->clear();
	Deadliner.Reset();
	ExpirationCounter = 0;
	if (HoldOpen)
	{
		KeyToItemCuckooMap::locked_table HoldOpenLocked = HoldOpen->lock_table();
		for (std::pair<FSkeletonKey, TWeakObjectPtr<AInstancedMeshManager>> RemainingKey : HoldOpenLocked)
		{
			TWeakObjectPtr<AInstancedMeshManager> Hold = RemainingKey.second;
			if (Hold != nullptr)
			{
				Hold->CleanupInstance(RemainingKey.first);
			}
		}
	}
	
	if (HoldOpenManagers)
	{
		// ReSharper disable once CppTemplateArgumentsCanBeDeduced - removing "redundant" template typing causes internal compiler error
		for (TTuple<FString, TWeakObjectPtr<AInstancedMeshManager>> RemainingKey : *HoldOpenManagers.Get())
		{
			AInstancedMeshManager* Hold = RemainingKey.Value.Get();
			if (Hold)
			{
				Hold->SwarmKineManager->ClearInternalFlags(EInternalObjectFlags::Async);
				Hold->ClearInternalFlags(EInternalObjectFlags::Async);
				Hold->ConditionalBeginDestroy();
			}
		}
	}

	ProjectileKeyToMeshManagerMapping->clear();
	MeshAssetToMeshManagerMapping->Empty();
	Super::Deinitialize();
}

UArtilleryProjectileDispatch::UArtilleryProjectileDispatch(): ProjectileDefinitions(nullptr), MyDispatch(nullptr)
{
	ExpirationCounter = 0; //just to make it clear.
	ManagerKeyToMeshManagerMapping = MakeShareable(new TMap<FSkeletonKey, TWeakObjectPtr<AInstancedMeshManager>>());
	ProjectileKeyToMeshManagerMapping = MakeShareable(new KeyToItemCuckooMap());
	ProjectileNameToMeshManagerMapping = MakeShareable(new TMap<FName, TWeakObjectPtr<AInstancedMeshManager>>());
	MeshAssetToMeshManagerMapping = MakeShareable(new TMap<FString, TWeakObjectPtr<AInstancedMeshManager>>());
	ProjectileToGunMapping = MakeShareable(new KeyToGunMap());
}

UArtilleryProjectileDispatch::~UArtilleryProjectileDispatch()
{
}

FProjectileDefinitionRow* UArtilleryProjectileDispatch::GetProjectileDefinitionRow(const FName ProjectileDefinitionId)
{
	if (ProjectileDefinitions != nullptr)
	{
		return ProjectileDefinitions->FindRow<FProjectileDefinitionRow>(ProjectileDefinitionId, TEXT("ProjectileTableLibrary"));
	}
	return nullptr;
}

FSkeletonKey UArtilleryProjectileDispatch::QueueProjectileInstance(const FName ProjectileDefinitionId,
                                                                   const FGunKey& Gun, const FVector3d& StartLocation,
                                                                   const FVector3d& MuzzleVelocity, const float Scale,
                                                                   Layers::EJoltPhysicsLayer Layer,
                                                                   TArray<FGameplayTag>* TagArray)
{
	if (IsReady){}
	TWeakObjectPtr<AInstancedMeshManager>* MeshManagerPtr = ProjectileNameToMeshManagerMapping->Find(ProjectileDefinitionId);
	if (MeshManagerPtr != nullptr && MeshManagerPtr->IsValid())
	{
		TWeakObjectPtr<AInstancedMeshManager> MeshManager = *MeshManagerPtr;
		FSkeletonKey ProjectileKey = MeshManager->GenerateNewProjectileKey();
		MyDispatch->RequestRouter->Bullet(ProjectileDefinitionId, StartLocation, Scale, MuzzleVelocity, ProjectileKey,
		                                  Gun, MyDispatch->GetShadowNow(), Layer);
		if (TagArray != nullptr)
		{
			FConservedTags TagContainer = MyDispatch->GetOrRegisterConservedTags(ProjectileKey);
			if (TagContainer.IsValid())
			{
				for (FGameplayTag& Tag : *TagArray)
				{
					MyDispatch->AddTagToEntity(ProjectileKey, Tag);
				}
			}
		}
		return ProjectileKey;
	}
	return FSkeletonKey();
}

FSkeletonKey UArtilleryProjectileDispatch::CreateProjectileInstance(FSkeletonKey ProjectileKey, FGunKey Gun,
                                                                    const FName ProjectileDefinitionId,
                                                                    const FTransform& WorldTransform,
                                                                    const FVector3d& MuzzleVelocity,
                                                                    const float Scale,
                                                                    const bool IsSensor,
                                                                    const bool IsDynamic,
                                                                    Layers::EJoltPhysicsLayer Layer,
                                                                    const bool CanExpire,
                                                                    const int LifeInTicks)
{
	if (IsReady)
	{
		TWeakObjectPtr<AInstancedMeshManager>* MeshManagerPtr = ProjectileNameToMeshManagerMapping->Find(ProjectileDefinitionId);
		if (MeshManagerPtr && MeshManagerPtr->IsValid())
		{
			TStrongObjectPtr<AInstancedMeshManager> MeshManager = MeshManagerPtr->Pin();
			if (MeshManager.IsValid())
			{
				FSkeletonKey NewProjectileKey = MeshManager->CreateNewInstance(
					WorldTransform, MuzzleVelocity, Layer, Scale, ProjectileKey, IsSensor, IsDynamic);
				ProjectileKeyToMeshManagerMapping->insert_or_assign(NewProjectileKey, *MeshManagerPtr);
				ProjectileToGunMapping->insert_or_assign(NewProjectileKey, Gun);
				
				UNiagaraParticleDispatch* NPD = GetWorld()->GetSubsystem<UNiagaraParticleDispatch>();
				TWeakObjectPtr<UNiagaraDataChannelAsset> ProjectileNDCAssetPtr = NPD->GetNDCAssetForProjectileDefinition(ProjectileDefinitionId);
				if (ProjectileNDCAssetPtr.IsValid())
				{
					NPD->RegisterKeyForProcessing(ProjectileDefinitionId, NewProjectileKey, ProjectileNDCAssetPtr);
				}
				
				if (CanExpire)
				{
					//TODO: revisit to provide rollback support. it'll be exactly like tombstones.
					int ExpireTicks = LifeInTicks == -1 ? DEFAULT_LIFE_OF_PROJECTILE : LifeInTicks;
					Deadliner.Add(ExpirationCounter+ExpireTicks, NewProjectileKey);
				}
				return NewProjectileKey;
			}
		}
	}
	return FSkeletonKey();
}

bool UArtilleryProjectileDispatch::IsArtilleryProjectile(const FSkeletonKey MaybeProjectile)
{
	return ProjectileKeyToMeshManagerMapping->contains(MaybeProjectile);
}

void UArtilleryProjectileDispatch::DeleteProjectile(const FSkeletonKey Target)
{
	if (UArtilleryDispatch::SelfPtr)
	{
		TWeakObjectPtr<AInstancedMeshManager> MeshManager;
		ProjectileKeyToMeshManagerMapping->find(Target, MeshManager);
		UArtilleryDispatch::SelfPtr->DeregisterGameplayTags(Target);
		if (MeshManager.IsValid())
		{
			MeshManager->CleanupInstance(Target);
			ProjectileKeyToMeshManagerMapping->erase(Target);
			ProjectileToGunMapping->erase(Target);
		}
		UNiagaraParticleDispatch::SelfPtr->CleanupKey(Target);
	}
}

TWeakObjectPtr<AInstancedMeshManager> UArtilleryProjectileDispatch::GetProjectileMeshManagerByManagerKey(
	const FSkeletonKey ManagerKey)
{
	TWeakObjectPtr<AInstancedMeshManager>* ManagerRef = ManagerKeyToMeshManagerMapping->Find(ManagerKey);
	return ManagerRef != nullptr ? *ManagerRef : nullptr;
}

TWeakObjectPtr<AInstancedMeshManager> UArtilleryProjectileDispatch::GetProjectileMeshManagerByProjectileKey(
	const FSkeletonKey ProjectileKey)
{
	TWeakObjectPtr<AInstancedMeshManager> ManagerRef;
	ProjectileKeyToMeshManagerMapping->find(ProjectileKey, ManagerRef);
	return ManagerRef;
}

TWeakObjectPtr<USceneComponent> UArtilleryProjectileDispatch::GetSceneComponentForProjectile(
	const FSkeletonKey ProjectileKey)
{
	TWeakObjectPtr<AInstancedMeshManager> ManagerRef;
	ProjectileKeyToMeshManagerMapping->find(ProjectileKey, ManagerRef);
	return ManagerRef.IsValid() ? ManagerRef->GetSceneComponentForInstance(ProjectileKey) : nullptr;
}

void UArtilleryProjectileDispatch::OnBarrageContactAdded(const BarrageContactEvent& ContactEvent)
{
	// We only care if one of the entities is a projectile
	if (UBarrageDispatch::SelfPtr)
	{
		// If you're lost, here's a quick example of how you can make use of the various attributes we expose here.
		// the point we hand along isn't guaranteed to be the only or best contact point, but it works well-ish.
		// if (ContactEvent.ContactEntity1.bIsPureHitbox || ContactEvent.ContactEntity2.bIsPureHitbox)
		// {
		// 	UE_LOG(LogTemp, Log, TEXT("One of these is a hitbox."));
		// 	if (ContactEvent.ContactEntity1.MyLayer == Layers::ENEMYHITBOX || ContactEvent.ContactEntity2.MyLayer == Layers::ENEMYHITBOX)
		// 	{
		// 		DrawDebugBox(GetWorld(), ContactEvent.PointIfAny, {40,40,40},FColor::Purple,false, 10);
		// 		UE_LOG(LogTemp, Log, TEXT("One of these is an enemy hitbox."));
		// 	}
		// }
		if (ContactEvent.IsEitherEntityAProjectile())
		{
			bool Body1_IsBullet = ContactEvent.ContactEntity1.bIsProjectile;
			// if both are bullets, if their layers allow it, we will collide them.
			// this is actually how antimissile works.
			//Oh and these are FBarrageKeys
			FBarrageKey ProjectileKey = Body1_IsBullet
				                     ? ContactEvent.ContactEntity1.ContactKey
				                     : ContactEvent.ContactEntity2.ContactKey;
			FBarrageKey EntityHitKey = Body1_IsBullet
				                    ? ContactEvent.ContactEntity2.ContactKey
				                    : ContactEvent.ContactEntity1.ContactKey;

			//we defer this as late as we can to minimize contention during the sim step.
			//don't make this a ref unless you want a very bad time.
			FBLet quickfib = UBarrageDispatch::SelfPtr->GetShapeRef(ProjectileKey);
			bool ProbablyValid = FBarragePrimitive::IsNotNull(quickfib);
			if (ProbablyValid)
			{
				FSkeletonKey KeyIntoArtillery = quickfib->KeyOutOfBarrage;
				quickfib.Reset();
				if (KeyIntoArtillery.IsValid())
				{
					FGunKey GunKey;
					bool found = ProjectileToGunMapping->find(KeyIntoArtillery, GunKey);
					if (found)
					{
						TSharedPtr<FArtilleryGun> ProjectileGun = MyDispatch->GetPointerToGun(GunKey);
						if (ProjectileGun && UBarrageDispatch::SelfPtr)
						{
							auto hold = UBarrageDispatch::SelfPtr->GetShapeRef(EntityHitKey);
							if (hold)
							{
								FSkeletonKey EntityKeyIntoArtillery = hold->KeyOutOfBarrage;
								if (EntityKeyIntoArtillery.IsValid())
								{
									ProjectileGun.Get()->ProjectileCollided(KeyIntoArtillery, EntityKeyIntoArtillery);
								}
							}
						}
					}
					// This works though!
					UArtilleryLibrary::TombstonePrimitive(KeyIntoArtillery);
				}
			}
		}
	}
}
