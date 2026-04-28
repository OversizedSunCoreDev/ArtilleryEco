#pragma once
#include "ConservedStream.hpp"
#include "SkeletonTypes.h"

//BLK Ring is an extension of our existing ConservedStream that uses what I've been calling a FAFO [sic] queuing model
//and a producer-authored consumer assignment model to produce a lockless and atomic-free design.
//You read that right.

//It takes a lot of memory but it's worth it.
//this is a specialized form because to be honest, it's easier to understand C++ code that doesn't make super heavy use of templates
//and I think the actual need for this in our codebase is quite limited in many ways. So this version uses transforms, but it's a general
//pattern. Everything involved needs to be both trivially copiable and trivially destructible. Think of it as flat records with no foreign keys.
namespace BLK
{
	using namespace ExportTemplateStream;
	static constexpr uint32_t MaxNumberOfConsumers = 32;
	static constexpr uint32_t MaxNumberOfProducers = 32;
	
	static constexpr uint8 MaxFallBehind = 128; //we don't support less than 1fps.
	

	
	struct FBoneArrayRecord
	{
		FSkeletonKey key = FSkeletonKey::Invalid();
		uint32 start = 0;
		char count = 0;
		unsigned char tick = 0;
		char hash = 0;
		char state = 0;
	};


	class BLKRingProducerSideBuffer
	{
	public:
		static constexpr uint32_t SaneAverageBonesPerSkeleton = 80;
		static constexpr uint32_t MaxNumberOfConsumers = 8;
		static constexpr uint32_t LayersLocked = 1;
		static constexpr uint32_t InputRecordsRequired = 9999;
		static constexpr uint32_t InputConservationWindow = (InputRecordsRequired/4) * SaneAverageBonesPerSkeleton;
		static constexpr uint32_t AddressableInputConservationWindow = InputConservationWindow - (LayersLocked * MaxNumberOfConsumers * SaneAverageBonesPerSkeleton);

		class  FBoneStream : public FConservedStream<AddressableInputConservationWindow,
		InputConservationWindow,
		FTransform,
		FTransform>
		{
		public:
			virtual void Add(UE::Math::TTransform<double> shell, long SentAt) override 
			{
				//right. tick data is stored on the record side. hoo.
				CurrentHistory[highestInput] = shell;
				++highestInput;
			}
			virtual void Add(UE::Math::TTransform<double> shell) override 
			{
				Add(shell, 0);
			}
		};


	
		class FMetaStream : public FConservedStream<InputRecordsRequired,
		(InputRecordsRequired + 2*MaxNumberOfConsumers),
		FBoneArrayRecord,
		FBoneArrayRecord>
		{
		public:
			virtual void Add(FBoneArrayRecord shell, long SentAt) override 
			{
				CurrentHistory[highestInput] = shell;
				CurrentHistory[highestInput].tick = SentAt % MaxFallBehind;//reserve a few states
				//This is mostly used for debugging.
				CurrentHistory[highestInput].state = 1; 
	
				++highestInput;
			}
			virtual void Add(FBoneArrayRecord shell) override 
			{
				Add(shell, 0);
			}
			
			
		};
		
		FMetaStream Records;
		FBoneStream Bones;
		
	};
	
	//The Blackout Lockless Kibitz-free (BLK) Ring is a ring of ring buffers, with consumer responsibilities divided by a hash ring modulo. 
	// Blackout refers to the process of maintaining a deadzone between the read and write heads, effectively using space as
	// time by calculating the max permissible imbalance between producer and consumer. Without the use of atomics, this is a probabilitic
	// stopping approach, but with the default constants on display here, the chances of a failed stop are lower than a hash collision.
	
	//Locks are not used at any point in the design. We use a set of atomics to coordinate at the start of the job, but after that
	//we are "kibitz-free." No cross-talk occurs during running jobs. This is achieved using the hash ring modulo design
	//common to DHTs. In fact, this is very close to being a hash table if you look at it sideways.
	
	//I won't lie. It's a strange design, built out of a number of normal lockless designs. I don't think it's necessarily
	//novel, but I've never seen it used elsewhere. It is fairly niche for reasons you'll see, but there's a chance it may be new.
	
	//This class exists to bind them all... Err... It exists to provide an example of the BLK Ring offering
	//N producer, M consumer streaming for variable sized records with no locks and no atomics. In our case, we use
	//it for moving transforms around, especially grouped transforms like sets of bones.
	
	//To achieve this, we pay a quite significant memory cost, and do some very weird things.
	//
	// first, this is based on designs from all over, from capn proto to various existing approaches.
	// It's not quite the same as any I'm aware of, because we can guarantee both trivial copy and trivial destruct.
	// I'm not claiming to have invented this, but I can't for the life of me remember seeing it before. On the other hand,
	// I probably read about a data structure every couple weeks and probably have for the last 20 years. If this is your
	// design, please let me know! --JMK!
	
	// BLK Ring works by giving each CONSUMER a modulo value that represents their slice of a hash ring using the hash ring modulo
	// design common to almost all distributed hash tables. Each producer gets its own buffer. 
	// 
	// production is pretty simple:
	// Each record gets a hash generated and committed by the producer
	// the record is written to the stream, in this case the bones stream, then the meta data is recorded in the records
	// 
	// We write the timestamp last, then set the state. as a result, we allow false negatives but no false positives or incomplete records.
	// to consume, a consumer sweeps through the record stream of each producer's buffer, starting where it left off last time
	// then checks to see if the hash falls into its part of the hash ring. if so, it uses it, if not, it doesn't.
	//
	// we use a small deadzone to let the system know that we've begun to write over ourselves. this region is not addressable
	// and is relatively large. this ensures that in practical cases, a record will never be read while partially written, without
	// using any atomics. in fact, we use no atomics at all, instead using a bit of thread local state here and there, and
	// incurring the cost of reviewing all records on all consumers. fortunately, this is quite small, and we only perform work
	// if the record matches our modulo. this means that even if the producers are uneven, the hash should be, which means that
	// consumer workload is kept even without explicit round robining or similar, and requires no communication. only the intended
	// recipient will ever read a record, so we don't need to do things like mark them read. likewise, because we do need to look at
	// each record with each consumer, and our records are trivially destructible, we don't need a shared value for read heads.
	/*
	 * These work across worlds but not across BLK prototypes. 
	 * These should generally be thread local and hermetically contained in the task itself. This means that you should
	 * NOT reuse them between producer and consumer or between BLK instances if you want them to work with multiple worlds.
	 * If your worlds have different numbers of producers or consumers from each other, may god have mercy on your soul.
	 */
	struct WorkerStateBundle
	{
		static constexpr int8_t INVALID = -1;
		//threads will generally only have one of these two.
		int8_t MyBufferAssignment = INVALID;
		int8_t MyModuloAssignment = INVALID;
		
		int64 MyGenerationMarker = INVALID; //todo: add wipe check in the thread loc handling.
	};
	struct InstanceConsumerState
	{
		int8_t BufferRingBookmark = 0;
		//TODO: main should have the commit that converts this to an int. double check post merge.
		uint64 PerBufferBookmarks[MaxNumberOfProducers] = {};
	};
	typedef std::pair<int8_t, std::optional<FBoneArrayRecord>> RecordFetchState;
	struct TransientQueuedDataRange
	{
		uint64 StartIndex = 0;
		uint64 Cur = 0;
		int64 Num = 0; // we actually only use one byte of this, so if you need to store other stuff.
		int8_t BufferRingBookmark = 0;
	};
	class BLKRing
	{
	public:
		//basically, producers don't need bookmarks, so we can get away with tracking their
		//state in the thread state bundle. Thread
		BLKRingProducerSideBuffer ProducerBuffers[MaxNumberOfProducers]; //weirdly, that's... that!
		InstanceConsumerState ConsumerRecords[MaxNumberOfConsumers]; //weirdly, that's... that!
		std::atomic_int8_t BufferAssignment = 0; //weirdly, that's -- naw, don't worry.
		std::atomic_int8_t ModuloAssignment = 0;
		int8_t CountOfConsumers = 0; //it's fine if this wraps.
		int64_t GenerationMarker = 1;// starts at first generation.
		BLKRing()
		{
			StartGenerationCountFromTime(); // this prevents a case where the gamethread outlives everything and still has
			// a thread local state ticket. If we just start at 1 each time we come up, the generation count will be fine
			// even though it's literally from a different world entirely. because time flows forward in most cases
			// the safest solution I could find that doesn't collide is to grab the unix epox time in microseconds.
		}

		int8_t UpdateConsumersAtStartOfTick()
		{
			//Count is a count.
			return CountOfConsumers = atomic_load_explicit(&ModuloAssignment, std::memory_order::acq_rel);
		}
		
		int8_t UpdateConsumersAtStartOfTick(int8 KnownCount)
		{
			//Count is a count.
			return CountOfConsumers = KnownCount;
		}
		//Increments generation, resets all state.
		
		void StartGenerationCountFromTime()
		{
			GenerationMarker = getSlicedMicrosecondNow();
		}
		
		void Reset()
		{
			CountOfConsumers = 0; //consumers will stop consuming at this point.
			BufferAssignment.store(0);
			ModuloAssignment.store(0);
			
			GenerationMarker++; //assignments are now ready to reset.
			for (auto& producer : ProducerBuffers)
			{
				producer.Bones.highestInput = 0;
				producer.Records.highestInput = 0;
			}
			for (auto& consumer : ConsumerRecords)
			{
				consumer.BufferRingBookmark = 0;
				for (int i = 0; i < MaxNumberOfProducers; ++i)
				{
					consumer.PerBufferBookmarks[i] = 0;
				}
			}
		}
		void ResetConsumersOnly_NoGenerationIncrement()
		{
			CountOfConsumers = 0; //consumers will stop consuming at this point.
			ModuloAssignment.store(0);
			
			//GENERATION IS NOT INCREMENTED
			//reset consumers.
			for (auto& consumer : ConsumerRecords)
			{
				consumer.BufferRingBookmark = 0;
				for (int i = 0; i < MaxNumberOfProducers; ++i)
				{
					consumer.PerBufferBookmarks[i] = 0;
				}
			}
		}
		
		FTransform* IterateRange(TransientQueuedDataRange& CurrentRange_In_Modified)
		{
			
			auto tar = CurrentRange_In_Modified.Cur++;
			auto got = ProducerBuffers[CurrentRange_In_Modified.BufferRingBookmark].Bones.get(tar);
			return got.has_value() ? &got.value() : nullptr;
		}
		
		int8_t GetMyProducerBuffer(WorkerStateBundle& ThreadStateTicket)
		{
			ThreadStateTicket.MyGenerationMarker = GenerationMarker;
			ThreadStateTicket.MyBufferAssignment = BufferAssignment++;// we actually want to return the unincremented version, so it goes 0, 1, 2... 
			return ThreadStateTicket.MyBufferAssignment;
			//++BufferAssignment would return 1,2,3...
		}
		
		//Consumer set up must be completed between processing cycles, in a "tick-like" way.
		//otherwise buffer bookmarks or Consumers at start of tick may be wrong, which won't crash
		//but may violate ExactlyOnce execution expectations. once all consumers are set up for a tick, UpdateConsumersAtStart
		//of tick should be called.
		//TODO: this limitation can be removed but I don't think it's worth reasoning about at the moment.
		int8_t GetMyConsumerModulo(WorkerStateBundle& ThreadStateTicket)
		{
			ThreadStateTicket.MyModuloAssignment = ModuloAssignment++;
			int counter = 0;
			//when adding a consumer, we need to make sure they don't sweep through literally everything all over again
			for (auto& a : ProducerBuffers)
			{
				ConsumerRecords[ThreadStateTicket.MyModuloAssignment].PerBufferBookmarks[counter] = a.Records.GetHighestGuaranteedInput();
				++counter;
			}
			ConsumerRecords[ThreadStateTicket.MyModuloAssignment].BufferRingBookmark = 0;
			ThreadStateTicket.MyGenerationMarker = GenerationMarker;
			return ThreadStateTicket.MyModuloAssignment;
		}
		bool CheckForInvalidModuloAssignment(WorkerStateBundle& ThreadStateTicket)
		{
			if (ThreadStateTicket.MyModuloAssignment == ThreadStateTicket.INVALID || ThreadStateTicket.MyGenerationMarker != GenerationMarker)
			{
				return true; 
			}
			return false;
		}
		
		bool CheckForInvalidBufferAssignment(WorkerStateBundle& ThreadStateTicket)
		{
			if (ThreadStateTicket.MyBufferAssignment == ThreadStateTicket.INVALID || ThreadStateTicket.MyGenerationMarker != GenerationMarker)
			{
				return true; 
			}
			return false;
		}
		//returns true if change was needed. Generally used witha thread_local but not always.
		[[nodiscard]] BARRAGE_API bool UpdateConsumerModulo(WorkerStateBundle& ThreadStateTicket)
		{
			if (CheckForInvalidModuloAssignment(ThreadStateTicket))
			{
				GetMyConsumerModulo(ThreadStateTicket);
				return true;// update required
			}
			return false;
		}
		BARRAGE_API bool UpdateBufferAssignment(WorkerStateBundle& ThreadStateTicket)
		{
			if (CheckForInvalidBufferAssignment(ThreadStateTicket))
			{
				GetMyProducerBuffer(ThreadStateTicket);
				return true;// update required
			}
			return false;
		}
		BARRAGE_API bool AddBones(FSkeletonKey Key, FTransform* ThisIsSafe, uint16_t count, WorkerStateBundle& ThreadStateTicket, uint64 tick);
		BARRAGE_API RecordFetchState GetMyNextRecord(WorkerStateBundle& ThreadStateTicket, uint64 UpToThisTick);
		BARRAGE_API TransientQueuedDataRange GetBoneIterator(RecordFetchState& BoneTicket, WorkerStateBundle& ThreadStateTicket);
		
	private:
		static int64 getSlicedMicrosecondNow()
		{
			using namespace std::chrono;
			return duration_cast<duration<int64, std::micro>>(system_clock::now().time_since_epoch()).count();
		}
	};
}
