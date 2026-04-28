// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Containers/CircularBuffer.h"
#include "BristleconeCommonTypes.h"
#include "UBristleconeWorldSubsystem.h"
#include <optional>
#include <unordered_map>
#include <ArtilleryShell.h>
#include "ArtilleryCommonTypes.h"
#include "ConservedStream.hpp"
#include "FActionPattern.h"
#include "KeyedConcept.h"
#include "TransformDispatch.h"

#include "CanonicalInputStreamECS.generated.h"

//TODO: finish adding the input streams, replace the local handling in Bristle54 character with references to the input stream ecs
//TODO: begin work on the conceptual frame for reconciling and assessing what input does and does not exist.
//AXIOMS: Input we send is real. Input we get in the batches represents the full knowledge of the server.
//AXIOM: A missed batch means we could be starting to desynchronize.
//AXIOM: Bristlecone Time + the fixed cadencing allows us to know when something SHOULD arrive.
//FACT: A batch missing input of OURS that we believe should be there could mean we are starting to desynchronize.
//FACT: A batch containing input of OURS that we believe should have been in an EARLIER batch could mean we are starting to desynchronize.
//FACT: The server will always have less of our input than we do.
//FACT: We may see input from other players before the server processes it.
//FACT: However, if it's missing from a batch, the server hasn't seen it yet.
//FACT: We may get server update pushes older than our input and/or older than the newest batch we have.
//FACT: We may have different orderings.
//FACT: we can determine the correct ordering and we all know the correct ordering of all input that made it into batches.

//Notes:
/*
Multiversus just relays input, it's all deterministic, and it records it.
At the end of the game, if clients disagree, it spins up a simulation and replays the inputs,
and uses that for the outcome (and presumably flags whoever disagreed for statistical detection).
*/

class UFireControlMachine;

static const uint32_t InputConservationWindow = 8192;
static const uint32_t AddressableInputConservationWindow = InputConservationWindow - (2 * TheCone::LongboySendHertz);
using ExportTemplateStream::FConservedStream;
template class FConservedStream<AddressableInputConservationWindow,
InputConservationWindow,
FArtilleryShell,
INNNNCOMING>;
UCLASS()
class ARTILLERYRUNTIME_API UCanonicalInputStreamECS : public UTickableWorldSubsystem, public ISkeletonLord, public ICanReady
{
	GENERATED_BODY()

public:
	constexpr static int OrdinateSeqKey = UBristleconeWorldSubsystem::OrdinateSeqKey + ORDIN::Step;
	virtual bool RegistrationImplementation() override;
	virtual void PostInitialize() override;
	/**
	 * Conserved input streams record their last 8,192 inputs. Yes, that's a few megs across all streams. No, it doesn't really seem to matter.
	 * Currently, this is for debug purposes, but we can use it with some additional features to provide a really expressive
	 * model for rollback at a SUPER granular level if needed. UE has existing rollback tech, though so...
	 *
	 * These streams are designed for single reader, single producer, but it is hypothetically possible to have a format where you do
	 * single producer, single consumer, multiple observer. I don't recommend that, due to the need to mark records as played for cosmetics.
	 *
	 * It is possible that an observer might end up with a stale view of if a record's cosmetic effects have been applied, in this circumstance.
	 * This is DONE DURING THE GET. That can lead to an unholy mess. If you need an observer, ensure that it does not regard cosmetics as important
	 * AND use a PEEK.
	 */

	//*************************************************
	//required for child classes. unfortunately, there's likely to be templating in them eventually
	//so we can't put them in the CPP if we want all compilers to behave at all times.
	//so everything, practically, is here.
	//I think this can be cleaned up in a couple weeks, as of 6/11/24. Let's see if I ever get to it. <3 JMK
	ArtilleryTime Now()
	{
		return MyNetworkDispatch->Now();
	};

	ActorKey ActorByStream(InputStreamKey Stream);
	InputStreamKey StreamByActor(ActorKey Stream);
	static inline UCanonicalInputStreamECS* SelfPtr = nullptr;
	InputStreamKey GetStreamForPlayer(PlayerKey);
	bool registerPattern(IPM::CanonPattern ToBind, FActionPatternParams FCM_Owner_ActorParams);
	bool removePattern(IPM::CanonPattern ToBind, FActionPatternParams FCM_Owner_ActorParams);
	TPair<ActorKey, InputStreamKey> RegisterKeysToParentActorMapping(FireControlKey MachineKey, bool IsActorForLocalPlayer, const ActorKey ParentKey);

	//this is the most portable way to do a folding region in C++.
#ifndef ARTILLERYECS_CLASSES_REGION_MARKER
public:

	
	
	class ARTILLERYRUNTIME_API FConservedInputPatternMatcher
	{
		InputStreamKey MyStream; //and may god have mercy on my soul.
		friend class FArtilleryBusyWorker;
		UCanonicalInputStreamECS* ECS;
		
	public:
		FConservedInputPatternMatcher(InputStreamKey StreamToLink, UCanonicalInputStreamECS* ParentECS)
		{
			AllPatternBinds = TMap<ArtIPMKey, TSharedPtr<TMap<FActionBitMask, FActionPatternParams>>>();
			AllPatternsByName = TMap<ArtIPMKey, IPM::CanonPattern>();
			MyStream=StreamToLink;
			ECS = ParentECS;
		}

		//there's a bunch of reasons we use string_view here, but mostly, it's because we can make them constexprs!
		//so this is... uh... pretty fast!
		TMap<ArtIPMKey, TSharedPtr<TMap<FActionBitMask, FActionPatternParams>>> AllPatternBinds;
		//broadly, at the moment, there is ONE pattern matcher running
		
		//this array is never made smaller.
		//there should only ever be about 10 patterns max,
		//and it's literally more expensive to remove them.
		//As a result, we track what's actually live via the binds
		//and this array is just lazy accumulative. it means we don't ever allocate a key array for TMap.
		//has some other advantages, as well.
		TArray<ArtIPMKey> Names;

		//same with this set, actually. patterns are stateless, and few. it's inefficient to destroy them.
		//instead we check binds.
		TMap<ArtIPMKey, IPM::CanonPattern> AllPatternsByName;
		
		//***********************************************************
		//
		// THIS IS THE IMPORTANT FUNCTION.
		//
		// ***********************************************************
		//
		// This makes things run. it doesn't correctly handle really anything, that's the busyworker's job
		// There likely will only be 12 or 18 FCMs runnin EVER because I think we'll want to treat each AI
		//faction pretty much as a single FCM except for a few bosses.
		//
		//hard to say. we might need to revisit this if the FCMs prove too heavy as full actor components.

		void runOneFrameWithSideEffects(bool isResim_Unimplemented,
		                                //USED TO DEFINE HOW TO HIDE LATENCY BY TRIMMING LEAD-IN FRAMES OF AN ARTILLERYGUN
		                                uint32_t leftTrimFrames,
		                                //USED TO DEFINE HOW TO SHORTEN ARTILLERYGUNS BY SHORTENING TRAILING or INFIX DELAYS, SUCH AS DELAYED EXPLOSIONS, TRAJECTORIES, OR SPAWNS, TO HIDE LATENCY.
		                                uint32_t rightTrimFrames,
		                                uint64_t InputCycleNumber,
		                                TArray<TPair<ArtilleryTime, EventBufferInfo>>&
		                                IN_PARAM_REF_TRIPLEBUFFER_LIFECYLEMANAGED
		                                //frame's a misnomer, actually.
		)
		{
			if (!isResim_Unimplemented)
			{
				UE_LOG(LogTemp, Display, TEXT("Still no resim, actually."));
			}

			//while the pattern matcher lives in the stream, the stream instance is not guaranteed to persist
			//In fact, it may get "swapped" and so we actually indirect through the ECS, grab the current stream whatever it is
			//then pin it. at this point, we can be sure that we hold A STREAM that DOES exist.
			//TODO: settle on a coherent error handling strategy here.
			TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> Stream = ECS->GetStream(MyStream);
			
			//the lack of reference (&) here causes a _copy of the shared pointer._ This is not accidental.
			for (TPair<ArtIPMKey, TSharedPtr<TMap<FActionBitMask, FActionPatternParams>>>& SetTuple : AllPatternBinds)
			{
				if (SetTuple.Value->Num() > 0)
				{
					IPM::CanonPattern currentPattern = AllPatternsByName[SetTuple.Key];
					FActionBitMask Union;
					TMap<FActionBitMask, FActionPatternParams>* currentSet = SetTuple.Value.Get();
					//TODO: remove and replace with a version that uses all bits set.
					//lot of refactoring to do that. let's get this working first.
					//for (FActionPatternParams& Elem : *currentSet)
					for (TPair<FActionBitMask, FActionPatternParams>& Elem : *currentSet)
					{
						//todo: replace with toFlat(). ffs.
						Union.buttons |= Elem.Value.ToSeek.buttons;
					}
					uint32_t result = currentPattern->runPattern(InputCycleNumber, Union, Stream);
					if (result)
					{
						//for (FActionPatternParams& Elem : *currentSet)
						for (TPair<FActionBitMask, FActionPatternParams>& Elem : *currentSet)
						{
							FActionBitMask& ToSeek = Elem.Value.ToSeek;
							if (ToSeek.getFlat() != 0 && (ToSeek.getFlat() & result) == ToSeek.getFlat())
							{
								BristleTime time = Stream->peek(InputCycleNumber)->SentAt;
								//THIS IS NOT SUPER SAFE. HAHAHAH. YAY.
								EventBufferInfo EventInfo;
								EventInfo.GunKey = Elem.Value.ToFire;
								EventInfo.Action = currentPattern->getName();
								EventInfo.ActionBitMask = ToSeek;
								IN_PARAM_REF_TRIPLEBUFFER_LIFECYLEMANAGED.Add(TPair<ArtilleryTime, EventBufferInfo>(
										time,
										EventInfo));
							}
						}
					}
				}
			}
		}
	};

	 

	
	class ARTILLERYRUNTIME_API FConservedInputStream
		: public ExportTemplateStream::FConservedStream
	<AddressableInputConservationWindow,
	InputConservationWindow,
	FArtilleryShell,
	INNNNCOMING>
	{
	public:
		UCanonicalInputStreamECS* MyDispatch;
		InputStreamKey MyKey;

		virtual std::optional<FArtilleryShell> get(uint64_t input) override
		{
			return FConservedStream<7936, 8192, FArtilleryShell, unsigned long long>::
				get(input);
		}

		virtual std::optional<FArtilleryShell> peek(uint64_t input) override
		{
			return FConservedStream<7936, 8192, FArtilleryShell, unsigned long long>::
				peek(input);
		}

		virtual ~FConservedInputStream() 
		{
		}

		//Mom?
		friend class FArtilleryBusyWorker;
		//Dad?
		friend class UCanonicalInputStreamECS;
		TSharedPtr<UCanonicalInputStreamECS::FConservedInputPatternMatcher> MyPatternMatcher;

		ActorKey GetActorByInputStream()
		{
			return MyDispatch->ActorByStream(MyKey); // this lets us avoid exposing the key.
		};
		
		explicit FConservedInputStream(UCanonicalInputStreamECS* LF_ECSParent, InputStreamKey ToBe)
		{
			MyDispatch = LF_ECSParent;
			MyKey = ToBe;
			MyPatternMatcher = MakeShareable(new UCanonicalInputStreamECS::FConservedInputPatternMatcher(ToBe, MyDispatch));
		}
		virtual void Add(INNNNCOMING shell, long SentAt) override
		{
			CurrentHistory[highestInput].MyInputActions = shell;
			CurrentHistory[highestInput].ReachedArtilleryAt = MyDispatch->Now();
			CurrentHistory[highestInput].SentAt = SentAt;
			//this is gonna get weird after a couple refactors, but that's why we hide it here.
		
			// reading, adding one, and storing are all separate ops. a slice here is never dangerous but can be erroneous.
			// because this is a volatile variable, it cannot be optimized away and most compilers will not reorder it.
			// however, volatile is basically useless normally. it doesn't provoke a memory fence, and for a variety of reasons
			// it's not suitable for most driver applications. However.
			// 
			// There's a special case which is a monotonically increasing value that is only ever
			// incremented by one thread with a single call site for the increment. In this case, you can still get
			// interleaved but the value will always be either k or k+1. If it's stale in cache, the worst case
			// is that the newest input won't be legible yet and this can be resolved by repolling.
			++highestInput;
		};

		virtual void Add(INNNNCOMING shell) override
		{
			CurrentHistory[highestInput].MyInputActions = shell;
			CurrentHistory[highestInput].ReachedArtilleryAt = MyDispatch->Now();
			CurrentHistory[highestInput].SentAt = MyDispatch->Now();
			++highestInput;
		}
	};

	//Used in the busyworker
	//There should only be one of these fuckers.
	//This is really just a way of grouping some of the functionality
	//of the overarching world subsystem together into an FClass that can
	//be used safely off thread without any consideration. you can use Uclasses if you're careful but...
#endif

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	TSharedPtr<FConservedInputStream> getNewStreamConstruct( PlayerKey ByPlayerConcept);
	TMap<PlayerKey, InputStreamKey> SessionPlayerToStreamMapping;

	TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> GetStream(InputStreamKey StreamKey) const;
	
	TSharedPtr<TArray<FArtilleryShell>> Get15LocalHistoricalInputs()
	{
		InputStreamKey streamkey = GetStreamForPlayer(PlayerKey::CABLE);
		TSharedPtr<UCanonicalInputStreamECS::FConservedInputStream> sptr = GetStream(streamkey);
		TSharedPtr<TArray<FArtilleryShell>> Inputs = MakeShareable(new TArray<FArtilleryShell>);
		if(sptr)
		{
			for(int i = 0; i <= 15; ++i)
			{
				std::optional<FArtilleryShell> input = sptr.Get()->peek( sptr->GetHighestGuaranteedInput());
				Inputs->Add(input.has_value() ? input.value() : FArtilleryShell());
			}
		}
		return Inputs;
	}
	
private:
	TMap<InputStreamKey, TSharedPtr<FConservedInputStream>> StreamKeyToStreamMapping;
	TMap<ActorKey, FireControlKey> LocalActorToFireControlMapping;
	TMap<InputStreamKey, ActorKey> StreamToActorMapping;
	TMap<ActorKey, InputStreamKey> ActorToStreamMapping;
	
	UPROPERTY()
	TObjectPtr<UBristleconeWorldSubsystem> MyNetworkDispatch; // World Subsystems are the last to go, making this a fairly safe idiom. ish.
};

typedef UCanonicalInputStreamECS UCISArty;
typedef UCISArty::FConservedInputStream ArtilleryControlStream;
typedef ArtilleryControlStream FAControlStream;
