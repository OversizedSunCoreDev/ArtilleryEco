//Hey jake, why isn't this templated?
//I don't wanna.


#pragma once
#include <array>
#include <mutex>

#include "CompileTimeStrings.h"
#include "SkeletonTypes.h"
#include "Memory/IntraTickThreadblindAlloc.h"

//this forms the basis. it only knows how to add to queues, it doesn't know what to do next.
//TODO: this can be made MUCH smaller by setting queues to roundrobin and spinning if contended.
//that only blocks in the case of three simultaneous producers, and blocks only very briefly, making it a perfect
//usecase for an atomic spin. As we currently never have three simultaneous producers, this is sufficent for us.
template<class Payload>
class LOCOMOCORE_API FParallelFixedQueue
{
protected:
	//if we have perf issues here due to allocs, we can swap over to the pooling allocator.
	//that's not trivial, though.
	//auto VectorAllocator = DATAlloc_StampedKeyVector(2*1014*1024);
	constexpr static size_t Width = 2; // producer threads supported.
	constexpr static size_t QueueHeight = 256; //yeaaaaaah, this is a problem.....
	constexpr static char FreeLock = -1;
	std::thread::id Consumer;
	std::atomic_char KeyMaker = 0;
	using FBlock = std::array<Payload, QueueHeight>;
	std::array<FBlock, Width> Queues;
	std::array<std::atomic_char, Width> Spinlocks;
	//you might notice these are not atomics. oh boy. you're in for a frickin treat brudder
	//we don't guarantee expirations will be picked up in one tick. this is why we don't really
	//support expirations shorter than 2 ticks. if you need that, what are you doing?
	uint32_t NowIndex = 0;
	//if you expire more than 2^60 things, I salute you. nevermind triggering an actual roll over.
	std::array<uint64_t, Width> BelievedWaterMarks = {};
	std::array<uint64_t, Width> WaterMarks = {};
public:
	bool Add(uint16_t MajorOrder, FSkeletonKey MinorOrder)
	{
		static thread_local char MyIndex;
		static thread_local char QueueToCheck;
		static thread_local std::once_flag bound;
		std::call_once(bound, [this]
		{
			MyIndex = KeyMaker++;
			WaterMarks[MyIndex] = 0;
			QueueToCheck = 0;
			BelievedWaterMarks[MyIndex] = 0;
			if (MyIndex >= Width)
			{
				return false; //c'mon...
			}
			return true;
		});
		char captured = MyIndex;
		while (captured != FreeLock)
		{
			//per standard:
			//Atomically compares the value representation of *this with that of expected.
			//If those are bitwise-equal, replaces the former with desired (performs read-modify-write operation).
			//
			//Otherwise, loads the actual value stored in *this into expected (performs load operation).

			//Here, we set captured to the expected. if the spinlock for our queue to check == FreeLock,
			//expected remains freelock and the spinlock for our queue to check is set to our index.
			//that lets us out of the while loop. otherwise, captured is set to the current value of the spinlock
			//and we're stuck in this loop a bit longer. because we roundrobin, the chances of actually blocking
			//are vanishingly low in our use cases, where queue adds are very fast so 3+ simultaneous producers is rare
			//and short-lived. Because the lock is relinquished before control of execution is returned, we cannot
			//actually end up in a self-recurrent state within a given thread, and so this simple pattern is sufficient.
			captured = FreeLock; // set this to expected. 
			QueueToCheck = (QueueToCheck+1) % Width; //round-robins.
			Spinlocks[QueueToCheck].compare_exchange_strong(captured, MyIndex);
		}
		Queues[MyIndex][WaterMarks[MyIndex]%QueueHeight] = {MajorOrder, MinorOrder};
		++WaterMarks[MyIndex];
		Spinlocks[QueueToCheck] = FreeLock;
		return false;
	}

	FParallelFixedQueue()
	{
		for (int i = 0; i < Width; ++i)
		{
			Spinlocks[i] = FreeLock;
		}
	}
};
template class FParallelFixedQueue<FStampedKeyPair>;
//sparse sequenced queue-like with threading support.
//This defines a specific usecase for the parallelfixedqueue
//default is 2p1c. you can generalize, like we do with the feedmaps
//but this is extremely fast even compared to those.
class LOCOMOCORE_API FParallelFixedSequencingQueue : public FParallelFixedQueue<FStampedKeyPair>
{
	std::unordered_map<uint32_t, std::vector<FSkeletonKey>> ByTick;

public:
	//fails if adding to the now or max
	//waits if anything else is adding.
	explicit FParallelFixedSequencingQueue()
		: ByTick(128) //one second worth of ticks.
	{
	}
	std::vector<FSkeletonKey> UpdateAndConsume();
	void Reset();
};

//this uses the fact that templates get unique statics to allow you to spec out your deadliners.
template<CompTimeStr id> class LOCOMOCORE_API SingletonDeadliner : public FParallelFixedSequencingQueue
{
public:
	
};
