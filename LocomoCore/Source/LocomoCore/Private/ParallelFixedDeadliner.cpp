#include "Structures/ParallelFixedDeadliner.h"
//this is all a hodgepodge variation on a https://en.wikibooks.org/wiki/More_C%2B%2B_Idioms/Nifty_Counter
#include <mutex>

std::vector<FSkeletonKey> FParallelFixedSequencingQueue::UpdateAndConsume()
{
	static thread_local std::once_flag bound;
	std::call_once(bound, [this]
	{
		Consumer = std::this_thread::get_id();
	});

	//once owned, you MUST use the same thread for updates and consumes.
	if (Consumer != std::this_thread::get_id())
	{
		throw;
	}

	for (int Q = 0; Q < Width; ++Q)
	{
		_mm_prefetch(reinterpret_cast<char const*>(&Queues[Q][BelievedWaterMarks[Q]]), _MM_HINT_T1);
	}
	for (int Q = 0; Q < Width; ++Q)
	{
		auto& bind = Queues[Q];
		for (; BelievedWaterMarks[Q] < WaterMarks[Q]; ++BelievedWaterMarks[Q])
		{
			auto& Mush = Queues[Q][BelievedWaterMarks[Q]%QueueHeight];
			auto TickToExpireOrMinimumDuration = std::max(Mush.first, NowIndex);
			if (ByTick[TickToExpireOrMinimumDuration].empty())
			{
				ByTick[TickToExpireOrMinimumDuration].reserve(8);
			}
			ByTick[TickToExpireOrMinimumDuration].push_back(Mush.second);
		}
	}
	auto Hold = ByTick[NowIndex];
	ByTick.erase(NowIndex);
	++NowIndex;
	return Hold;
}

void FParallelFixedSequencingQueue::Reset()
{
	NowIndex=0;
	for (auto& a : WaterMarks)
	{
		a = 0;
	}
	for (auto& a : BelievedWaterMarks)
	{
		a = 0;
	}
	ByTick.clear();
}
