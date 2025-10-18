#pragma once
#include "CoreMinimal.h"
#include "EAttributes.h"
#include "ConservedTagContainer.h"
#include "SkeletonTypes.h"


#include "RequestRouterTypes.generated.h"

enum ArtilleryRequestType
{
	FireAGun,
	GetAnUnboundGun,
	GetABullet,
	ModifyGun,
	CreateATicklite,
	BindAI,
	// Particles
	ParticleSystemActivateOrDeactivate,
	SpawnParticleSystemAttached,
	SpawnParticleSystemAtLocation,
	// Meshes
	SpawnInstancedStaticMesh,
	TagReferenceModel,
	NoTagReferenceModel,
};

USTRUCT()
struct ARTILLERYRUNTIME_API FRequestThing
{
	GENERATED_BODY()
	FRequestThing(): Stamp(0), ThingVector(), ThingVector2(), ThingVector3(), ThingRotator(), Relationship(), Layer(),
	                 Type()
	{
	}

	FRequestThing(ArtilleryRequestType MyType): Stamp(0), ThingVector(), ThingVector2(), ThingVector3(), ThingRotator(),
	                                            Relationship(), Layer()
	{
		if (MyType == FireAGun || MyType == GetABullet)
		{
			throw; // these requests MUST be made with the inheritor type FRequestGameThreadThing
			//if it didn't crash here, it'd crash when the request was run.
		}
		Type = MyType;
	}

	ArtilleryRequestType GetType()
	{
		return Type;
	}
	
	ArtilleryTime Stamp;
	FGunKey Gun;
	FSkeletonKey SourceOrSelf;
	FSkeletonKey TargetOrNonSelfAffected;
	FName ThingName;
	FVector ThingVector;
	FVector ThingVector2;
	FVector ThingVector3;
	FRotator ThingRotator;
    Arty::FARelatedBy Relationship;
	int TicksDuration = -1;
	bool ActivateIfPossible = true;
	bool CanExpire = true;
	Layers::EJoltPhysicsLayer Layer;
	//Do Not Touch This unless you know what you are doing.
	FConservedTags ConservedTags; 
	
protected:
	ArtilleryRequestType Type;
};

USTRUCT()
struct ARTILLERYRUNTIME_API FRequestGameThreadThing : public FRequestThing//Frick
{
	GENERATED_BODY()
	FRequestGameThreadThing()
	{
		
	}
	explicit FRequestGameThreadThing(ArtilleryRequestType MyType)
	{
		Type = MyType;
	}
};

USTRUCT()
struct ARTILLERYRUNTIME_API FGrantWith
{
	GENERATED_BODY()
	
	FGrantWith() : Requested(0)
	{}
	
	FGrantWith(ArtilleryTime Stamp) : Requested(Stamp)
	{
	}
	
	constexpr uint8 static Completed =	  0b00000001;
	constexpr uint8 static Eventual =     0b00000010;
	constexpr uint8 static Within1Tick =  0b00000100;
	constexpr uint8 static Bound =	      0b00001000;
	constexpr uint8 static GameThread =   0b00010000;
	constexpr uint8 static Nullable =     0b00100000;
	constexpr uint8 static WideCadence =  0b01000000;
	constexpr uint8 static AutoGun  =     0b10000000;
	
	FGrantWith Set(uint8 flags)
	{
		Conditions = flags;
		return *this;
	}
	
	FGrantWith Bind(FSkeletonKey key)
	{
		Key = key;
		return *this;
	}
	
	FSkeletonKey Key;
	ArtilleryTime Requested = 0;
	uint8	Conditions = 0;
};
