#include "ArtilleryControlComponent.h"
#include "ArtilleryDispatch.h"
#include "GameplayAbilitySpec.h"
#include "GameplayAbilitySpecHandle.h"
#include "FArtilleryGun.h"

void UArtilleryFireControl::PushGunToFireMapping(const FGunKey& ToFire)
{
	Arty::FArtilleryFireGunFromDispatch Inbound;
	Inbound.BindUObject(this, &UArtilleryFireControl::FireGun);
	MyDispatch->RegisterReady(ToFire, Inbound);
	MyGuns.Add(ToFire);
}

void UArtilleryFireControl::FireGun(TSharedPtr<FArtilleryGun> Gun, bool InputAlreadyUsedOnce, EventBufferInfo BufferInfo)
{
	if (Gun->Prefire != nullptr)
	{
		FGameplayAbilitySpec BackboneFiring = BuildAbilitySpecFromClass(
			(Gun->Prefire).GetClass(),
			0,
			-1);
		FGameplayAbilitySpecHandle FireHandle = BackboneFiring.Handle;
		Gun->PreFireGun(
			FireHandle,
			AbilityActorInfo.Get(),
			FGameplayAbilityActivationInfo(EGameplayAbilityActivationMode::Authority),
			BufferInfo);
	}
}

void UArtilleryFireControl::PopGunFromFireMapping(const FGunKey& ToRemove)
{
	MyDispatch->Deregister(ToRemove);
	MyGuns.Remove(ToRemove);
}

void UArtilleryFireControl::InitializeComponent()
{
	Super::InitializeComponent();
	MyKey = UArtilleryFireControl::orderInInitialize++;
	//we rely on attribute replication, which I think is borderline necessary, but I wonder if we should use effect replication.
	//historically, relying on gameplay effect replication has led to situations where key state was not managed through effects.
	//for OUR situation, where we have few attributes and many effects, huge amounts of effects are likely not interesting for us to replicate.
	ReplicationMode = EGameplayEffectReplicationMode::Minimal; 
}

void UArtilleryFireControl::ReadyForReplication()
{
	Super::ReadyForReplication();
	MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
}

void UArtilleryFireControl::BeginPlay()
{
	Super::BeginPlay(); 
	MyDispatch = GetWorld()->GetSubsystem<UArtilleryDispatch>();
}

void UArtilleryFireControl::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);
	for (FGunKey Gun : MyGuns)
	{
		MyDispatch->Deregister(Gun); // emergency deregister.
	}
	
	MyAttributes.Reset();
}
