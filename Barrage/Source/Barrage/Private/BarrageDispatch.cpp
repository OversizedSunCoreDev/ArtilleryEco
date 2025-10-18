#include "BarrageDispatch.h"

#include "BarrageContactEvent.h"
#include "IsolatedJoltIncludes.h"
#include "FWorldSimOwner.h"
#include "CoordinateUtils.h"
#include "FBPhysicsInput.h"

//https://github.com/GaijinEntertainment/DagorEngine/blob/71a26585082f16df80011e06e7a4e95302f5bb7f/prog/engine/phys/physJolt/joltPhysics.cpp#L800
//this is how gaijin uses jolt, and war thunder's honestly a pretty strong comp to our use case.

bool UBarrageDispatch::RegistrationImplementation()
{
	FScopeLock GrantFeedLock(&MultiAccLock);
	//locks the job system's threads on the grant worker func until we're spun up.
	UE_LOG(LogTemp, Warning, TEXT("Barrage:Subsystem: Online"));
	for (TSharedPtr<TArray<FBLet>>& TombFibletArray : Tombs)
	{
		TombFibletArray = MakeShareable(new TArray<FBLet>());
		TombFibletArray->Empty();
	}

	UE_LOG(LogTemp, Warning, TEXT("Barrage:TransformUpdateQueue: Online"));
	GameTransformPump = MakeShareable(new TransformUpdatesForGameThread(20024));
	ContactEventPump = MakeShareable(new TCircularQueue<BarrageContactEvent>(8192));
	FBarragePrimitive::GlobalBarrage = this;
	//this approach may actually be too slow. it is pleasingly lockless, but it allocs 16megs
	//and just iterating through that could be Rough for the gamethread.
	//so we may need to think about options.

	//using bind here is a bit opaque, so... binding a func means you specify its parameters. member functions have an implicit-this
	//parameter, if you think about if like, well, like I would. not proud of that, btw. the placeholder works like the marking system
	//used by UE delegates. Bind isn't exactly like a delegate though, because this actually performs a pretty slick typehiding trick
	//which allows us to cleanly break a dependency.
	std::function<void(int)> bind = std::bind(&UBarrageDispatch::GrantWorkerFeed, this, std::placeholders::_1);
	JoltGameSim = MakeShareable(new FWorldSimOwner(TickRateInDelta, bind));
	JoltBodyLifecycleMapping = MakeShareable(new KeyToFBLet());
	TranslationMapping = MakeShareable(new KeyToKey());
	SelfPtr = this;
	return true;
}

void UBarrageDispatch::GrantWorkerFeed(int MyThreadIndex)
{
	FScopeLock GrantFeedLock(&MultiAccLock);

	//TODO: expand if we need for rollback powers. could be sliiiick
	JoltGameSim->WorkerAcc[WorkerThreadAccTicker] = FBOutputFeed(std::this_thread::get_id(), 8192);
	MyWORKERIndex = WorkerThreadAccTicker;
	++WorkerThreadAccTicker;
}

void UBarrageDispatch::GrantClientFeed()
{
	FScopeLock GrantFeedLock(&GrowOnlyAccLock);

	//TODO: expand if we need for rollback powers. could be sliiiick
	JoltGameSim->ThreadAcc[ThreadAccTicker] = FWorldSimOwner::FBInputFeed(std::this_thread::get_id(), 8192);

	MyBARRAGEIndex = ThreadAccTicker;
	++ThreadAccTicker;
}

UBarrageDispatch::UBarrageDispatch()
{
}

UBarrageDispatch::~UBarrageDispatch()
{
	//now that all primitives are destructed
	FBarragePrimitive::GlobalBarrage = nullptr;
}

void UBarrageDispatch::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SET_INITIALIZATION_ORDER_BY_ORDINATEKEY_AND_WORLD
}

void UBarrageDispatch::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UBarrageDispatch::Deinitialize()
{
	SelfPtr = nullptr;
	Super::Deinitialize();
	JoltBodyLifecycleMapping = nullptr;
	TranslationMapping = nullptr;
	for (TSharedPtr<TArray<FBLet>>& TombFibletArray : Tombs)
	{
		TombFibletArray = nullptr;
	}

	TSharedPtr<TransformUpdatesForGameThread> HoldOpen = GameTransformPump;
	GameTransformPump = nullptr;
	if (HoldOpen)
	{
		if (HoldOpen.GetSharedReferenceCount() > 1)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "Hey, so something's holding live references to the queue. Maybe. Shared Ref Count is not reliable."
			       ));
		}
		HoldOpen->Empty();
	}
	HoldOpen = nullptr;

	TSharedPtr<TCircularQueue<BarrageContactEvent>> EventPumpHoldOpen = ContactEventPump;
	ContactEventPump = nullptr;
	if (EventPumpHoldOpen)
	{
		if (EventPumpHoldOpen.GetSharedReferenceCount() > 1)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT(
				       "Hey, so something's holding live references to the contact queue. Maybe. Shared Ref Count is not reliable."
			       ));
		}
		EventPumpHoldOpen->Empty();
	}
	EventPumpHoldOpen = nullptr;
}

void UBarrageDispatch::SphereCast(
	double Radius,
	double Distance,
	FVector3d CastFrom,
	FVector3d Direction,
	TSharedPtr<FHitResult> OutHit,
	const JPH::BroadPhaseLayerFilter& BroadPhaseFilter,
	const JPH::ObjectLayerFilter& ObjectFilter,
	const JPH::BodyFilter& BodiesFilter,
	uint64_t timestamp)
{
	if (!CastFrom.ContainsNaN())
	{
		JoltGameSim->SphereCast(Radius, Distance, CastFrom, Direction, OutHit, BroadPhaseFilter, ObjectFilter,
		                        BodiesFilter);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Attempted to SphereCast with a NaN value in the CastFrom position! [%s]"),
		       *CastFrom.ToString());
	}
}

void UBarrageDispatch::SphereSearch(
	FBarrageKey ShapeSource,
	FVector3d Location,
	double Radius,
	const JPH::BroadPhaseLayerFilter& BroadPhaseFilter,
	const JPH::ObjectLayerFilter& ObjectFilter,
	const JPH::BodyFilter& BodiesFilter,
	uint32* OutFoundObjectCount,
	TArray<uint32>& OutFoundObjects)
{
	JPH::BodyID CastingBodyID;
	if (!JoltGameSim->BarrageToJoltMapping->find(ShapeSource, CastingBodyID))
	{
		CastingBodyID = JPH::BodyID();
		// invalid BUT characters HAVE NO FLESSSSSSSH BLEHHHHHH (seriously, without an inner shape, they lack a body)
	}
	JoltGameSim->SphereSearch(CastingBodyID, Location, Radius, BroadPhaseFilter, ObjectFilter, BodiesFilter,
	                          OutFoundObjectCount, OutFoundObjects);
}

void UBarrageDispatch::CastRay(
	FVector3d CastFrom,
	FVector3d Direction,
	const JPH::BroadPhaseLayerFilter& BroadPhaseFilter,
	const JPH::ObjectLayerFilter& ObjectFilter,
	const JPH::BodyFilter& BodiesFilter,
	TSharedPtr<FHitResult> OutHit)
{
	if (!CastFrom.ContainsNaN())
	{
		JoltGameSim->CastRay(CastFrom, Direction, BroadPhaseFilter, ObjectFilter, BodiesFilter, OutHit);
	}
}

//Defactoring the pointer management has actually made this much clearer than I expected.
//these functions are overload polymorphic against our non-polymorphic POD params classes.
//this is because over time, the needs of these classes may diverge and multiply
//and it's not clear to me that Shapefulness is going to actually be the defining shared
//feature. I'm going to wait to refactor the types until testing is complete.
FBLet UBarrageDispatch::CreatePrimitive(FBBoxParams& Definition, FSkeletonKey OutKey, uint16_t Layer, bool isSensor,
                                        bool forceDynamic, bool isMovable)
{
	if (JoltGameSim)
	{
		FBarrageKey temp = JoltGameSim->CreatePrimitive(Definition, Layer, isSensor, forceDynamic, isMovable);
		return ManagePointers(OutKey, temp, Box);
	}
	return nullptr;
}

FBLet UBarrageDispatch::CreateProjectile(FBBoxParams& Definition, FSkeletonKey OutKey, uint16_t Layer)
{
	if (JoltGameSim)
	{
		FBarrageKey temp = JoltGameSim->CreatePrimitive(Definition, Layer, true, true);
		return ManagePointers(OutKey, temp, Projectile);
	}
	return nullptr;
}

FBLet UBarrageDispatch::CreatePrimitive(FBCharParams& Definition, FSkeletonKey OutKey, uint16_t Layer)
{
	if (JoltGameSim)
	{
		FBarrageKey temp = JoltGameSim->CreatePrimitive(Definition, Layer);
		return ManagePointers(OutKey, temp, FBShape::Character);
	}
	return nullptr;
}

FBLet UBarrageDispatch::CreatePrimitive(FBSphereParams& Definition, FSkeletonKey OutKey, uint16_t Layer, bool isSensor)
{
	if (JoltGameSim)
	{
		FBarrageKey temp = JoltGameSim->CreatePrimitive(Definition, Layer, isSensor);
		return ManagePointers(OutKey, temp, FBShape::Sphere);
	}
	return nullptr;
}

FBLet UBarrageDispatch::CreatePrimitive(FBCapParams& Definition, FSkeletonKey OutKey, uint16 Layer, bool isSensor,
                                        FMassByCategory::BMassCategories MassClass)
{
	if (JoltGameSim)
	{
		FBarrageKey temp = JoltGameSim->CreatePrimitive(Definition, Layer, isSensor, MassClass);
		return ManagePointers(OutKey, temp, Capsule);
	}
	return nullptr;
}

FBLet UBarrageDispatch::ManagePointers(FSkeletonKey OutKey, FBarrageKey temp, FBShape form) const
{
	//interestingly, you can't use auto here. don't try. it may allocate a raw pointer internal
	//and that will get stored in the jolt body lifecycle mapping. 
	//it basically ensures that you will get turned into a pillar of salt.
	FBLet indirect = MakeShareable(new FBarragePrimitive(temp, OutKey));
	indirect->Me = form;
	JoltBodyLifecycleMapping->insert_or_assign(indirect->KeyIntoBarrage, indirect);
	TranslationMapping->insert_or_assign(indirect->KeyOutOfBarrage, indirect->KeyIntoBarrage);
	return indirect;
}

//https://github.com/jrouwe/JoltPhysics/blob/master/Samples/Tests/Shapes/MeshShapeTest.cpp
//probably worth reviewing how indexed triangles work, too : https://www.youtube.com/watch?v=dOjZw5VU6aM
FBLet UBarrageDispatch::LoadComplexStaticMesh(FBTransform& MeshTransform,
                                              const UStaticMeshComponent* StaticMeshComponent,
                                              FSkeletonKey OutKey,
                                              bool IsSensor)
{
	if (JoltGameSim)
	{
		FBLet shared = JoltGameSim->LoadComplexStaticMesh(MeshTransform, StaticMeshComponent, OutKey);
		if (shared && shared.IsValid())
		{
			shared->Me = FBShape::Static;
			JoltBodyLifecycleMapping->insert_or_assign(shared->KeyIntoBarrage, shared);
			TranslationMapping->insert_or_assign(shared->KeyOutOfBarrage, shared->KeyIntoBarrage);
			return shared;
		}
	}
	return nullptr;
}


//https://github.com/jrouwe/JoltPhysics/blob/master/Samples/Tests/Shapes/MeshShapeTest.cpp
//probably worth reviewing how indexed triangles work, too : https://www.youtube.com/watch?v=dOjZw5VU6aM
FBLet UBarrageDispatch::LoadEnemyHitboxFromStaticMesh(FBTransform& MeshTransform,
											  const UStaticMeshComponent* StaticMeshComponent,
											  FSkeletonKey OutKey, bool IsSensor, bool UseRawMeshForCollision, FVector CenterOfMassTranslation) 
{
	if (JoltGameSim)
	{
		FBLet shared = JoltGameSim->LoadComplexStaticMesh(MeshTransform, StaticMeshComponent, OutKey,
			Layers::ENEMYHITBOX, JoltGameSim->LayerToMotionTypeMapping(Layers::ENEMYHITBOX), IsSensor, UseRawMeshForCollision, CenterOfMassTranslation);
		if (shared && shared.IsValid())
		{
			FBarragePrimitive::SetGravityFactor(0, shared);
			shared->Me= FBShape::Complex;
			JoltBodyLifecycleMapping->insert_or_assign(shared->KeyIntoBarrage, shared);
			TranslationMapping->insert_or_assign( shared->KeyOutOfBarrage, shared->KeyIntoBarrage);
			return shared;
		}
	}
	return nullptr;
}

//unlike our other ecs components in artillery, barrage dispatch does not maintain the mappings directly.
//this is because we may have _many_ jolt sims running if we choose to do deterministic rollback in certain ways.
//This is a copy by value return on purpose, as we want the ref count to rise.
FBLet UBarrageDispatch::GetShapeRef(FBarrageKey Existing) const
{
	//SharedPTR's def val is nullptr. this will return nullptr as soon as entomb succeeds.
	//if entomb gets sliced, the tombstone check will fail as long as it is performed within 27 milliseconds of this call.
	//as a result, one of three states will arise:
	//1) you get a null pointer
	//2) you get a valid shared pointer which will hold the asset open until you're done, but the tombstone markings will be set, letting you know
	//that this thoroughfare? it leads into the region of peril.
	//3) you get a valid shared pointer which will hold the asset open until you're done, but the markings are being set
	//this means your calls will all succeed but none will be applied during the apply shadow phase.
	TSharedPtr<KeyToFBLet> holdopen = JoltBodyLifecycleMapping;
	FBLet out;
	//TODO: This line feels SUSPICIOUS!
	return holdopen && holdopen->find(Existing, out) ? out : nullptr;
}

FBLet UBarrageDispatch::GetShapeRef(FSkeletonKey Existing) const
{
	//SharedPTR's def val is nullptr. this will return nullptr as soon as entomb succeeds.
	//if entomb gets sliced, the tombstone check will fail as long as it is performed within 27 milliseconds of this call.
	//as a result, one of three states will arise:
	//1) you get a null pointer
	//2) you get a valid shared pointer which will hold the asset open until you're done, but the tombstone markings will be set, letting you know
	//that this thoroughfare? it leads into the region of peril.
	//3) you get a valid shared pointer which will hold the asset open until you're done, but the markings are being set
	//this means your calls will all succeed but none will be applied during the apply shadow phase.
	TSharedPtr<KeyToKey> holdopen = TranslationMapping;
	if (holdopen)
	{
		FBarrageKey key = 0;
		if (holdopen->find(Existing, key))
		{
			TSharedPtr<KeyToFBLet> HoldMapping = JoltBodyLifecycleMapping;
			if (HoldMapping)
			{
				FBLet deref;
				if (HoldMapping->find(key, deref))
				{
					if (FBarragePrimitive::IsNotNull(deref)) //broken out to ease debug.
					{
						return deref;
					}
				}
			}
		}
	}
	return nullptr;
}

void UBarrageDispatch::FinalizeReleasePrimitive(FBarrageKey BarrageKey)
{
	if (JoltGameSim)
	{
		JoltGameSim->FinalizeReleasePrimitive(BarrageKey);
	}
}

TStatId UBarrageDispatch::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBarrageDispatch, STATGROUP_Tickables);
}

void UBarrageDispatch::StackUp()
{
	uint16_t RefilledUpTo = 0;
	uint16_t Adding = 0;
	if (JoltGameSim)
	{
		//accumulate.
		for (FWorldSimOwner::FBInputFeed WorldSimOwnerFeedMap : JoltGameSim->ThreadAcc)
		{
			//the threadmaps themselves are always allocated, but they may not be "valid"
			const TSharedPtr<FWorldSimOwner::FBInputFeed::ThreadFeed> HoldOpenThreadQueue = WorldSimOwnerFeedMap.Queue;
			if (WorldSimOwnerFeedMap.Queue && ((HoldOpenThreadQueue != nullptr)) && WorldSimOwnerFeedMap.That !=
				std::thread::id()) //if there IS a thread.
			{
				while (HoldOpenThreadQueue.Get() && !HoldOpenThreadQueue->IsEmpty())
				{
					const FBPhysicsInput* input = HoldOpenThreadQueue->Peek();
					InternalSortableSet[RefilledUpTo] = *input;

					++RefilledUpTo;
					HoldOpenThreadQueue->Dequeue();
				}
			}
		}

		//process.
		for (int i = 0; i < RefilledUpTo; ++i)
		{
			auto& input = InternalSortableSet[i];
			//handle adds first!
			if (input.Action == PhysicsInputType::ADD)
			{
				Adds[Adding] = JPH::BodyID(input.Target.KeyIntoBarrage); // oh JESUS
				++Adding;
			}
		}
		// std::sort(Adds.data(), Adding....); add sort here once we have our ordering worked out.

		JPH::BodyInterface* BodyInt = JoltGameSim->body_interface;
		//adding in one batch MASSIVELY reduces thread contention and the amount of quadtree messiness.
		//it'd be honestly nice to batch the creation as well, but that hasn't actually been showing up in
		//our execution cost metrics. instead, I think we're just getting be-beefed by the mess.
		if (Adding)
		{
			JPH::BodyInterface::AddState state = BodyInt->AddBodiesPrepare(Adds.data(), Adding);
			BodyInt->AddBodiesFinalize(Adds.data(), Adding, state, JPH::EActivation::Activate);
		}
		for (int i = 0; i < RefilledUpTo; ++i)
		{
			auto& input = InternalSortableSet[i];
			JPH::BodyID result;
			const bool bID = JoltGameSim->BarrageToJoltMapping->find(input.Target, result);
			if (bID && input.metadata == FBShape::Character)
			{
				UpdateCharacter(const_cast<FBPhysicsInput&>(input));
			}

			if (bID && !result.IsInvalid() && input.metadata != FBShape::Character)
			{
				switch (input.Action)
				{
				case PhysicsInputType::ADD:
					//in retrospect, this should have just added another bloody queue set. ugh.
					//skip, already handled.
					break;
				case PhysicsInputType::Rotation:
					//prolly gonna wanna change this to add torque................... not sure.
					BodyInt->SetRotation(result, input.State, JPH::EActivation::Activate);
					break;
				case PhysicsInputType::OtherForce:
					BodyInt->AddForce(result, input.State.GetXYZ(), JPH::EActivation::Activate);
					break;
				case PhysicsInputType::Velocity:
					BodyInt->SetLinearVelocity(result, input.State.GetXYZ());
					break;
				case PhysicsInputType::SetPosition:
					BodyInt->SetPosition(result, input.State.GetXYZ(), JPH::EActivation::Activate);
					break;
				case PhysicsInputType::SelfMovement:
					BodyInt->AddForce(result, input.State.GetXYZ(), JPH::EActivation::Activate);
					break;
				case PhysicsInputType::AIMovement:
					BodyInt->AddForce(result, input.State.GetXYZ(), JPH::EActivation::Activate);
					break;
				case PhysicsInputType::SetGravityFactor:
					BodyInt->SetGravityFactor(result, input.State.GetZ());
					break;
				default:
					UE_LOG(LogTemp, Warning,
					       TEXT("UBarrageDispatch::StackUp: Unimplemented handling for input action [%d]"),
					       input.Action);
				}
			}
		}
	}
}

bool UBarrageDispatch::UpdateCharacters(TSharedPtr<TArray<FBPhysicsInput>> CharacterInputs) const
{
	return JoltGameSim->UpdateCharacters(CharacterInputs);
}

bool UBarrageDispatch::UpdateCharacter(FBPhysicsInput& CharacterInput) const
{
	return JoltGameSim->UpdateCharacter(CharacterInput);
}

void UBarrageDispatch::StepWorld(uint64 Time, uint64_t TickCount)
{
	TSharedPtr<FWorldSimOwner> PinSim = JoltGameSim;

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("Step World");
	if (this && JoltGameSim && PinSim)
	{
		if (TickCount % 128 == 0)
		{
			//TRACE_CPUPROFILER_EVENT_SCOPE_STR("Broadphase Optimize");
			PinSim->OptimizeBroadPhase();
		}

		CleanTombs();
		PinSim->StepSimulation();
		TSharedPtr<TMap<FBarrageKey, TSharedPtr<FBCharacterBase>>> HoldOpenCharacters = PinSim->CharacterToJoltMapping;
		if (HoldOpenCharacters)
		{
			// ReSharper disable once CppTemplateArgumentsCanBeDeduced - disabled to clear warning that causes compiler error if "fixed"
			for (TTuple<FBarrageKey, TSharedPtr<FBCharacterBase>> CharacterKeyAndBase : *HoldOpenCharacters)
			{
				if (CharacterKeyAndBase.Value->mCharacter)
				{
					if (CharacterKeyAndBase.Value->mCharacter->GetPosition().IsNaN())
					{
						CharacterKeyAndBase.Value->mCharacter->SetLinearVelocity(
							CharacterKeyAndBase.Value->World->GetGravity());
						CharacterKeyAndBase.Value->mCharacter->SetPosition(CharacterKeyAndBase.Value->mInitialPosition);
						CharacterKeyAndBase.Value->mForcesUpdate = CharacterKeyAndBase.Value->World->GetGravity();
					}
					CharacterKeyAndBase.Value->StepCharacter();
				}
			}
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE_STR("Jolt Body Lifecycle Update");
			//maintain tombstones
			TSharedPtr<KeyToFBLet> HoldCuckooLifecycle = JoltBodyLifecycleMapping;
			TSharedPtr<KeyToKey> HoldCuckooTranslation = TranslationMapping;
			if (HoldCuckooLifecycle && HoldCuckooLifecycle.Get() && !HoldCuckooLifecycle.Get()->empty())
			{
				KeyToFBLet::locked_table HoldCuckooLifecycleLocked = HoldCuckooLifecycle->lock_table();
				for (std::pair<const FBarrageKey, FBLet> KeyAndBarragePrimitive : HoldCuckooLifecycleLocked)
				{
					if (KeyAndBarragePrimitive.first.KeyIntoBarrage != 0)
					{
						FBLet HoldOpenFBP = KeyAndBarragePrimitive.second;
						//NOTE: nullity check here includes tombstone check. Hence the odd form of the SECOND check.
						//in other words, this checks != null && !tombstoned
						if (FBarragePrimitive::IsNotNull(HoldOpenFBP))
						{
							FBarragePrimitive::TryUpdateTransformFromJolt(HoldOpenFBP, Time);
							//returns a bool that can be used for debug.
						} //This checks for != null && tombstoned
						else if (HoldOpenFBP && HoldOpenFBP->tombstone != 0 && Tombs[TombOffset])
						//just to make it explicit.
						{
							//TODO: MAY NOT BE THREADSAFE. CHECK NLT 10/5/24
							Tombs[TombOffset]->Push(HoldOpenFBP);
						}
					}
				}
			}
		}
	}
}

bool UBarrageDispatch::BroadcastContactEvents() const
{
	if (GetWorld())
	{
		TSharedPtr<TCircularQueue<BarrageContactEvent>> HoldOpen = ContactEventPump;
		while (HoldOpen && !HoldOpen->IsEmpty())
		{
			const BarrageContactEvent* Update = HoldOpen->Peek();
			if (Update)
			{
				try
				{
					switch (Update->ContactEventType)
					{
					case EBarrageContactEventType::ADDED:
						OnBarrageContactAddedDelegate.Broadcast(*Update);
						break;
					case EBarrageContactEventType::PERSISTED:
						OnBarrageContactPersistedDelegate.Broadcast(*Update);
						break;
					case EBarrageContactEventType::REMOVED:
						// REMOVE EVENTS REQUIRE ADDITIONAL SPECIAL HANDLING AS THEY DO NOT HAVE ALL DATA SET
						OnBarrageContactRemovedDelegate.Broadcast(*Update);
						break;
					default:
						break;
					}
					HoldOpen->Dequeue();
				}
				catch (...)
				{
					return false; //we'll be back! we'll be back!!!!
				}
			}
		}
		return true;
	}
	return false;
}

inline BarrageContactEvent ConstructContactEvent(EBarrageContactEventType EventType, UBarrageDispatch* BarrageDispatch,
                                                 const JPH::Body& inBody1, const JPH::Body& inBody2,
                                                 JPH::ContactSettings& ioSettings, FVector point = {0,0,0})
{
	return BarrageContactEvent(
		EventType, BarrageContactEntity(BarrageDispatch->GenerateBarrageKeyFromBodyId(inBody1.GetID()), inBody1),
		BarrageContactEntity(BarrageDispatch->GenerateBarrageKeyFromBodyId(inBody2.GetID()), inBody2), point);
}

//retains the presence of the contact manifold, as we will eventually need to use this.
void UBarrageDispatch::HandleContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
                                          const JPH::ContactManifold& inManifold,
                                          JPH::ContactSettings& ioSettings)
{
	HandleContactAdded(inBody1, inBody2, ioSettings, FVector(CoordinateUtils::FromJoltCoordinates(inManifold.mBaseOffset + inManifold.mRelativeContactPointsOn1[0])));
}

void UBarrageDispatch::HandleContactAdded(const BarrageContactEntity Ent1, const BarrageContactEntity Ent2)
{
	BarrageContactEvent ContactEventToEnqueue = BarrageContactEvent(
		EBarrageContactEventType::ADDED, Ent1, Ent2);
	ContactEventPump->Enqueue(ContactEventToEnqueue);
}

void UBarrageDispatch::HandleContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
										  JPH::ContactSettings& ioSettings, FVector point)
{
	BarrageContactEvent ContactEventToEnqueue = ConstructContactEvent(EBarrageContactEventType::ADDED, this, inBody1,
																	  inBody2, ioSettings, point);
	ContactEventPump->Enqueue(ContactEventToEnqueue);
}

void UBarrageDispatch::HandleContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2,
                                              const JPH::ContactManifold& inManifold,
                                              JPH::ContactSettings& ioSettings)
{
	BarrageContactEvent ContactEventToEnqueue = ConstructContactEvent(EBarrageContactEventType::PERSISTED, this,
	                                                                  inBody1, inBody2, ioSettings);
	ContactEventPump->Enqueue(ContactEventToEnqueue);
}

void UBarrageDispatch::HandleContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) const
{
	//this is busted now prolly.
	FBarrageKey BK1 = this->GenerateBarrageKeyFromBodyId(inSubShapePair.GetBody1ID());
	FBarrageKey BK2 = this->GenerateBarrageKeyFromBodyId(inSubShapePair.GetBody2ID());
	BarrageContactEvent ContactEventToEnqueue(
		EBarrageContactEventType::REMOVED,
		BarrageContactEntity(BK1),
		BarrageContactEntity(BK2));
	ContactEventPump->Enqueue(ContactEventToEnqueue);
}

FBarrageKey UBarrageDispatch::GenerateBarrageKeyFromBodyId(const JPH::BodyID& Input) const
{
	return JoltGameSim->GenerateBarrageKeyFromBodyId(Input);
};

FBarrageKey UBarrageDispatch::GenerateBarrageKeyFromBodyId(const uint32 RawIndexAndSequenceNumberInput) const
{
	return JoltGameSim->GenerateBarrageKeyFromBodyId(RawIndexAndSequenceNumberInput);
}

JPH::DefaultBroadPhaseLayerFilter UBarrageDispatch::GetDefaultBroadPhaseLayerFilter(JPH::ObjectLayer inLayer) const
{
	return JoltGameSim->physics_system->GetDefaultBroadPhaseLayerFilter(inLayer);
}

JPH::DefaultObjectLayerFilter UBarrageDispatch::GetDefaultLayerFilter(JPH::ObjectLayer inLayer) const
{
	return JoltGameSim->physics_system->GetDefaultLayerFilter(Layers::CAST_QUERY);
}

JPH::SpecifiedObjectLayerFilter UBarrageDispatch::GetFilterForSpecificObjectLayerOnly(JPH::ObjectLayer inLayer)
{
	return JPH::SpecifiedObjectLayerFilter(inLayer);
}

JPH::IgnoreSingleBodyFilter UBarrageDispatch::GetFilterToIgnoreSingleBody(FBarrageKey ObjectKey) const
{
	JPH::BodyID CastingBodyID;
	JoltGameSim->GetBodyIDOrDefault(ObjectKey, CastingBodyID);
	return JPH::IgnoreSingleBodyFilter(CastingBodyID);
}

JPH::IgnoreSingleBodyFilter UBarrageDispatch::GetFilterToIgnoreSingleBody(const FBLet& ToIgnore) const
{
	return GetFilterToIgnoreSingleBody(ToIgnore->KeyIntoBarrage);
}

//Bounds are OPAQUE. do not reference them. they are protected for a reason, because they are
//subject to semantic changes. the Point is left in the UE space. 
FBBoxParams FBarrageBounder::GenerateBoxBounds(const FVector3d& point, double xDiam,
                                               double yDiam, double zDiam,
                                               const FVector3d& OffsetCenterToMatchBoundedShape,
                                               FMassByCategory::BMassCategories MyMassClass)
{
	FBBoxParams blob;
	blob.Point = point;
	blob.Offset.X = OffsetCenterToMatchBoundedShape.X;
	blob.Offset.Y = OffsetCenterToMatchBoundedShape.Y;
	blob.Offset.Z = OffsetCenterToMatchBoundedShape.Z;
	blob.JoltX = CoordinateUtils::DiamToJoltHalfExtent(xDiam);
	blob.JoltY = CoordinateUtils::DiamToJoltHalfExtent(zDiam); //this isn't a typo.
	blob.JoltZ = CoordinateUtils::DiamToJoltHalfExtent(yDiam);
	blob.MassClass = MyMassClass;
	return blob;
}

//Bounds are OPAQUE. do not reference them. they are protected for a reason, because they are
//subject to change. the Point is left in the UE space. 
FBSphereParams FBarrageBounder::GenerateSphereBounds(const FVector3d& point, double radius)
{
	FBSphereParams blob;
	blob.point = point;
	blob.JoltRadius = CoordinateUtils::RadiusToJolt(radius);
	return blob;
}

//Bounds are OPAQUE. do not reference them. they are protected for a reason, because they are
//subject to change. the Point is left in the UE space, signified by the UE type. 
FBCapParams FBarrageBounder::GenerateCapsuleBounds(const UE::Geometry::FCapsule3d& Capsule)
{
	FBCapParams blob;
	blob.point = Capsule.Center();
	blob.JoltRadius = CoordinateUtils::RadiusToJolt(Capsule.Radius);
	blob.JoltHalfHeightOfCylinder = CoordinateUtils::RadiusToJolt(Capsule.Extent());
	blob.taper = 0.f;
	return blob;
}

//Bounds are OPAQUE. do not reference them. they are protected for a reason, because they are
//subject to change. the Point is left in the UE space, signified by the UE type. 
FBCharParams FBarrageBounder::GenerateCharacterBounds(const FVector3d& point, double radius, double extent,
                                                      double speed)
{
	FBCharParams blob;
	blob.point = point;
	blob.JoltRadius = CoordinateUtils::RadiusToJolt(radius);
	blob.JoltHalfHeightOfCylinder = CoordinateUtils::RadiusToJolt(extent);
	blob.speed = speed;
	return blob;
}
