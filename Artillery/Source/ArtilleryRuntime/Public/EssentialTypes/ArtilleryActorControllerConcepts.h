#pragma once

#include "CoreMinimal.h"
#include "ArtilleryShell.h"
#include "UObject/ObjectMacros.h"
#include "KeyedConcept.h"
#include "ArtilleryActorControllerConcepts.generated.h"

//This is the set of CONCEPTS in the C++ sense. Traits, arguably. In artillery, anything can have an artillery tick, not just actors.
//This is already basically true in UE, but the various tickables aren't unified in the way you address and interact with them.
//There's really good reasons for this, because the problem UE solves is WAY more general. That's why we use interfaces-as-concepts
//so that it's easy to tie the two models of thinking together into something pretty harmonious.
//
//Makes it easy to just add an artillery tick onto something when doing integration, and know that you can mount it into the correct
//control flows.
//
//

//=-=-=-= =-=-=-==-=-=-= =-=-=-==-=-=-==-=-=-==-=-=-==-=-=-= =-=-=-= =-=-=-=
//=-=-=-=\=-=-=-==-=-=-=\=-=-=-==-=-=-==-=-=-==-=-=-==-=-=-=\=-=-=-=\=-=-=-=|
//
//  WARNING FROM THE PAST: Bear in mind, artillery ticks MAY OR MAY NOT run on the game thread.
//
//
//The danger is in a particular location... it increases towards a center... the center of danger is here... of a particular size and shape, and below us.
//The danger is still present, in your time, as it was in ours.
//The danger is to the threads, and it can kill.
//The form of the danger is determinism risks.
//The danger is unleashed only if you substantially disturb this place. But you might have to. lol. lmao.
//=-=-=-=|=-=-=-==-=-=-/-=-=-=-==-=-=-==-=-=-==-=-=-==-=-=-=\=-=-\=-=-=-=|-=
//=-=-=-= =-=-=-==-=-=-= =-=-=-==-=-=-==-=-=-==-=-=-==-=-=-= =-=-=-= =-=-=-=

//You can think of an artillery tick as a LITTLE like a Unity Fixed Update, except that because it's threaded,
//it's totally decoupled from the frame\UETick. They don't block each other, and artillery really only offers truly
//strong determinism guarantees for game simulation modifications on the artillery tick. This is..... a source of some problems.


//TICKHEAVY VS TICKLITE
//Artillery ticked controllites are separate from ticklites, much more expensive, and run concurrently with them.
//They are a kind of TICKHEAVY. Get it? Hahaha.. ha...

//pronounced Controllite. I hope that helped. :)
//This is the minimum required for artillery to express control over something.
//That might be an actor, it might be a component, it might not have a UObject at all.
//Could be a static mesh, even. It's somewhat unfortunate that this concept is coming to fruition so late in
//the development of the system, but I think as we tidy, it'll prove very useful.

//This is not simple stuff, and I apologize, but it's less screwy than what we had.

UINTERFACE()
class UArtilleryLocomotionInterface : public UKeyedConstruct
{
	GENERATED_UINTERFACE_BODY()

};

inline UArtilleryLocomotionInterface::UArtilleryLocomotionInterface(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

class IArtilleryLocomotionInterface : public IKeyedConstruct
{
	GENERATED_IINTERFACE_BODY()
	
public:
	/** Assigns Team Agent to given TeamID */
	virtual bool LocomotionStateMachine(FArtilleryShell PreviousMovement, FArtilleryShell Movement, bool RunAtLeastOnce, bool Smear)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Unexpected call to unimplemented Locomotion State Machine method. While not necessarily fatal, this is always worth checking."));
		return false; //looks like you called super? or lost an input.
	}
	virtual FSkeletonKey GetMyKey() const override
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Unexpected call to unimplemented GetMyKey method. While not necessarily fatal, this is always worth checking."));
		return FSkeletonKey();
	}

	virtual void PrepareForPossess() 
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Unexpected call to unimplemented PrepareForPossess method. Interesting, but less dangerous."));
	}

	virtual void PrepareForUnPossess() 
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Unexpected call to unimplemented PrepareForUnPossess method. Interesting, but less dangerous."));
	}	
	//at minimum, this will be bound to the artillery tick of the controller
	//right now, this is allowed to have side effects.
	virtual void LookStateMachine(FRotator& IN_OUT_LookAxisVector) {}

	virtual bool IsReady() { return false;}
};

UCLASS()
class UIArtilleryLocomotionDefault : public UObject, public IArtilleryLocomotionInterface
{
	GENERATED_BODY()
	
public:
	virtual bool LocomotionStateMachine(FArtilleryShell PreviousMovement, FArtilleryShell Movement, bool RunAtLeastOnce, bool Smear) override
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("UIArtilleryLocomotionDefault: Unexpected call to a default's method. While not necessarily fatal, this is always worth checking."));
		return false; //this means you lost an input, hit a concurrency bug, or did something exciting with inheritance.
	}
	
	//same here.
	virtual void LookStateMachine(FRotator& IN_OUT_LookAxisVector) override
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("UIArtilleryLocomotionDefault: Unexpected call to a default's method. While not necessarily fatal, this is always worth checking."));
	}
};

UINTERFACE()
class UTickHeavy : public UKeyedConstruct
{
	GENERATED_UINTERFACE_BODY()
};

inline UTickHeavy::UTickHeavy(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

class ITickHeavy : public IKeyedConstruct
{
	GENERATED_IINTERFACE_BODY()
	
public:
	//=-=-=-= =-=-=-==-=-=-= =-=-=-==-=-=-==-=-=-==-=-=-==-=-=-= =-=-=-= =-=-=-=
	//=-=-=-=\=-=-=-==-=-=-=\=-=-=-==-=-=-==-=-=-==-=-=-==-=-=-=\=-=-=-=\=-=-=-=|
	//
	//  WARNING FROM THE PAST: Bear in mind, artillery ticks MAY OR MAY NOT run on the game thread.
	//
	//=-=-=-= =-=-=-==-=-=-= =-=-=-==-=-=-==-=-=-==-=-=-==-=-=-= =-=-=-= =-=-=-=
	//=-=-=-=\=-=-=-==-=-=-=\=-=-=-==-=-=-==-=-=-==-=-=-==-=-=-=\=-=-=-=\=-=-=-=|
	virtual void ArtilleryTick(FArtilleryShell PreviousMovement, FArtilleryShell Movement, bool RunAtLeastOnce, bool Smear) {}
	
	virtual void ArtilleryTick(uint64_t TicksSoFar) {}
	virtual void ArtilleryTick() {}
	
	virtual FSkeletonKey GetMyKey() const override { return FSkeletonKey(); }
};

class FTickHeavy : public ITickHeavy
{
	//this is a dummy for now that just serves to create the base of the inheritance tree.
};

UINTERFACE()
class UArtilleryControllite : public UTickHeavy
{
	GENERATED_UINTERFACE_BODY()
};

inline UArtilleryControllite::UArtilleryControllite(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

//So, making a TickHeavy, huh? They're pretty expressive, but they carry some additional requirements about determinism
//and thread safety. You should probably try to use a ticklite first, but if you do end up needing a tickheavy,
//remember that you may want to use the Request Router found in NeedA.h - if you, ya know, Need A Thing.
//That automatically handles a bunch of the thread magic for you.
//Anyway, good luck! I'm proud of you. No, seriously. This stuff is hard, and it took me a long time. --J

//Here's the example use of the tickheavy. This is what helps drive the behavior-tree side of thistle, and to a lesser extent,
//we use this interface for the statetrees as well. Those are a little less cut and dry, though. You could use the base
//interface, instead of this one, unless you're in a similar control flow to the existing use of controllites.
class IArtilleryControllite: public ITickHeavy
{
	GENERATED_IINTERFACE_BODY()
	
public:
	/** This is a special special special boy, a most special boy.
	 *  Looks like you're about to make a TickHeavy. These are pretty hard to offer determinism over.
	 *  You're sure this can't be a ticklite?
	 * **/
	virtual void ArtilleryTick(FArtilleryShell PreviousMovement, FArtilleryShell Movement, bool RunAtLeastOnce, bool Smear) override {}
	
	virtual void RegisterWithDispatch(FSkeletonKey MyKey) {}
	virtual FSkeletonKey GetMyKey() const override { return FSkeletonKey(); }
};

class FControllite : public IArtilleryControllite
{
	//this is a dummy for now that just serves to create the base of the inheritance tree.
};

//this is used for defaulting.
UCLASS()
class UBrokenController : public UObject, public IArtilleryControllite
{
	GENERATED_BODY()
	
public:
	virtual void ArtilleryTick(FArtilleryShell PreviousMovement, FArtilleryShell Movement, bool RunAtLeastOnce, bool Smear) override {}
	
	virtual void RegisterWithDispatch(FSkeletonKey MyKey) override {}
	virtual FSkeletonKey GetMyKey() const override { return FSkeletonKey(); }
};
