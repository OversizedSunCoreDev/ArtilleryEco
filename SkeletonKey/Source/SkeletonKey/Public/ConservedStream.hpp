#pragma once
#include "NoGuaranteeReadOnly.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "Engine/DataTable.h"
#include "Containers/CircularBuffer.h"


namespace ExportTemplateStream
{
	template<int AddressableInputConservationWindow, int InputConservationWindow,
	typename DataOnlyTriviallyDestructibleValueType, typename Incoming>
	class FConservedStream : public FNoGuaranteeReadOnly<DataOnlyTriviallyDestructibleValueType>
	{
	public:
	
		static inline const int Invalid_Key = -1;

		FConservedStream()
		{
		} //broke the rule of five. still breaking it i guess but less badly.
		

		TCircularBuffer<DataOnlyTriviallyDestructibleValueType> CurrentHistory = TCircularBuffer<DataOnlyTriviallyDestructibleValueType>(InputConservationWindow);

		//Correct usage procedure is to null check then store a copy.
		//Failure to follow this procedure will lead to eventual misery.
		//This has a side-effect of marking the record as played at least once.
		virtual std::optional<DataOnlyTriviallyDestructibleValueType> get(uint64_t input)
		{
			// the highest input is a reserved write-slot.
			//the lower bound here ensures that there's always minimum two seconds worth of memory separating the readers
			//and the writers. How safe is this? It's not! But it's insanely fast. Enjoy, future jake!
			if (input >= highestInput || (highestInput - input) > AddressableInputConservationWindow)
			{
				return std::optional<DataOnlyTriviallyDestructibleValueType>(std::nullopt);
			}
			//CurrentHistory[input].FlagRun();// if there is a flag to set, set it.
			//this is the only risky op in here from a threading perspective.
			return std::optional<DataOnlyTriviallyDestructibleValueType>(CurrentHistory[input]);
		};

		//THE ONLY DIFFERENCE WITH PEEK IS THAT IT DOES NOT SET RUNATLEASTONCE.
		//Peek is public out of necessity, but generally, you should use get.
		virtual std::optional<DataOnlyTriviallyDestructibleValueType> peek(uint64_t input) override
		{
			// the highest input is a reserved write-slot.
			//the lower bound here ensures that there's always minimum two seconds worth of memory separating the readers
			//and the writers. How safe is this? It's not! But it's insanely fast. Enjoy, future jake!
			//TODO: Refactor this to use an atomic int instead of this hubristic madness.
			if (input >= highestInput || (highestInput - input) > AddressableInputConservationWindow)
			{
				return std::optional<DataOnlyTriviallyDestructibleValueType>(std::nullopt);
			}
			return std::optional<DataOnlyTriviallyDestructibleValueType>(CurrentHistory[input]);
		};
		
		

		uint64_t GetHighestGuaranteedInput()
		{
			return highestInput-1;
		}
		
		uint64_t highestInput = 0; // volatile is utterly useless for its intended purpose. 

		//Add can only be used by the Artillery Worker Thread through the methods of the UCISArty
		//or similar specializations like the one used for the transform pump
		virtual void Add(Incoming shell, long SentAt) = 0;

		//example add!
		// {
		// 	CurrentHistory[highestInput].MyInputActions = shell;
		// 	CurrentHistory[highestInput].ReachedArtilleryAt = ECSParent->Now();
		// 	CurrentHistory[highestInput].SentAt = SentAt;
		// 	//this is gonna get weird after a couple refactors, but that's why we hide it here.
		//
		// 	// reading, adding one, and storing are all separate ops. a slice here is never dangerous but can be erroneous.
		// 	// it doesn't provoke a memory fence, and for a variety of reasons
		//HOWEVER
		// 	// 
		// 	// There's a special case which is a monotonically increasing value that is only ever
		// 	// incremented by one thread with a single call site for the increment. In this case, you can still get
		// 	// interleaved but the value will always be either k or k+1. If it's stale in cache, the worst case
		// 	// is that the newest input won't be legible yet and this can be resolved by repolling.
		// 	++highestInput;
		// };

		//Overload for local add via feed from cabling. don't use this unless you are CERTAIN.
		virtual void Add(Incoming shell) = 0;


	};
}