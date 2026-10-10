#include "BLK.h"
#include "MashFunctions.h"


//what you hash determines what the distribution of the load to the consumers will be.
//if effectively random, you'll get round robin. if you hash something else, you may get
//other loading patterns. this can be useful, and we actually hash here by key, causing each 
//key to only ever be consumed by a single parallel consumer. this is necessary to prevent some fun bugs.
bool BLK::BLKRing::AddBones(FSkeletonKey Key, FTransform* ThisIsSafe, uint16_t count, WorkerStateBundle& ThreadStateTicket, uint64 tick)

{
	if (ThreadStateTicket.MyBufferAssignment < MaxNumberOfProducers && count > 0 && ThreadStateTicket.MyBufferAssignment != WorkerStateBundle::INVALID && ThreadStateTicket.MyBufferAssignment < MaxNumberOfProducers)
	{
		FBoneArrayRecord NewRecord;
		NewRecord.start = ProducerBuffers[ThreadStateTicket.MyBufferAssignment].Bones.highestInput;
		for (int i = 0; i < count; ++i)
		{
			ProducerBuffers[ThreadStateTicket.MyBufferAssignment].Bones.Add(*(ThisIsSafe+i));
		}
		
		NewRecord.key = Key;
		NewRecord.hash = FMMM::hash16_s6M8(Key);
		NewRecord.count = count;
		ProducerBuffers[ThreadStateTicket.MyBufferAssignment].Records.Add(NewRecord, tick);
		return true;
	}
	return false;
}

//this retrieves the next record marked for the consumer providing its thread state ticket.
//remember, the threadstate ticket is the representation of us as a consumer.
//this works by 
BLK::RecordFetchState 
BLK::BLKRing::GetMyNextRecord( WorkerStateBundle& ThreadStateTicket, uint64 UpToThisTick)
{
	//for legibility, take a ref to the bookmark
	auto& WhichBuffer = ConsumerRecords[ThreadStateTicket.MyModuloAssignment].BufferRingBookmark;
	//for each producer's buffer....
	
	// FIX (mod-by-zero + OOB index): if this consumer has no valid modulo, or no
	// consumers have been registered for this tick, there is nothing to consume
	// and the `% CountOfConsumers` below would divide by zero. Bail to EoF.
	// This also guards ConsumerRecords[...] against an INVALID (-1) index.
	const uint8_t Mod = ThreadStateTicket.MyModuloAssignment;
	if (Mod < 0 || Mod >= static_cast<int>(MaxNumberOfConsumers) || CountOfConsumers <= 0)
	{
		return RecordFetchState(-1, std::nullopt);
	}
	
	while (WhichBuffer < MaxNumberOfProducers)
	{
		if (ProducerBuffers[WhichBuffer].Records.highestInput == 0) {
			++WhichBuffer;
			continue;
		}
		
		//get the current top of the buffer.
		auto max = ProducerBuffers[WhichBuffer].Records.highestInput;
		//then run through the records looking for anything that matches our hash modulo.
		//each item is only checked once per consumer, as the consumer state maintains a bookmark into each producer.
		//This is why records are separate from blobs, they need to be very small and fast to page through

		//for the current buffer, run to the max by incrementing that SPECIFIC bookmark
		while ( ConsumerRecords[ThreadStateTicket.MyModuloAssignment].PerBufferBookmarks[WhichBuffer] < max)
		{
			
			auto CandidateRecord =  ProducerBuffers[ConsumerRecords[ThreadStateTicket.MyModuloAssignment].BufferRingBookmark].Records.get(ConsumerRecords[ThreadStateTicket.MyModuloAssignment].PerBufferBookmarks[WhichBuffer]);
			//NOTE: adding a prefetch on this line for the next iteration's record may be smart, but I'd like to think the compiler is smart enough to do that.
			//OTOH, that makes some assumptions that the compiler author may judge over-zealous.
			if (CandidateRecord.has_value())
			{
				// see https://medium.com/@tom_84912/monotonic-state-variables-the-fast-and-furious-of-ipc-communications-652a0faa5da8
				//diff = (F(t + Δt) — F(t) + (MAX+1)) modulo (MAX+1)
				// D <= MaxFallBehind
				// diff = (F(t + Δt) — F(t) + (MaxFallBehind+1)) modulo (MaxFallBehind+1)
				// Δt is the change in the time stamps. this allows missed wrap arounds but that's fine. the queue isn't big enough for them anyway.
				// thus as a result of the FAFO queue:
				// (UpToThisTick - CandidateRecord.value().tick + MaxFallBehind+1) % MaxFallBehind+1 WOULD be the minimized dist
				// except that UpToThisTick isn't modulo'd. Maddening, right?
				// so TPlusDelta = UpToThisTick%MaxFallBehind
				// (TPlusDelta - CandidateRecord.value().tick + MaxFallBehind+1) % MaxFallBehind+1 <= MaxFallBehind
				// is our distance wrap guard. next we want to specify a range where we consider a < b well defined.
				// We would like to say that if our min distance is higher than a certain amount, we don't care if a might be > b.
				// Because UpToThisTick and tick are assigned from the same monotone var
				// we can say with confidence that if tick > uptothis, we KNOW it's either wrapped or from a tick we shouldn't
				// consume yet. So, finally, we can set a heuristic value, H. so:
				// if((TPlusDelta - CandidateRecord.value().tick + MaxFallBehind+1) % MaxFallBehind+1 <= MaxFallBehind
				//    && (abs(TPlusDelta - CandidateRecord.value().tick) > H || CandidateRecord.value.tick < UpToThisTick)
				// interestingly, we now have a much simpler predicate by fallthrough iff H < ((MaxFallBehind/2) - 1)
				// AND our monotone is increasing.
				// (abs(TPlusDelta - CandidateRecord.value().tick) > H || CandidateRecord.value.tick < TPlusDelta)
				long long TPlusDelta = UpToThisTick%MaxFallBehind;
				//okay, so basically, this explodes if the high bit is set. but tick is mod max fall behind, so we're safe.
				long long dist = TPlusDelta - static_cast<int32>(CandidateRecord.value().tick);
				if (std::abs(dist) > HeuristicWrapWidth || CandidateRecord.value().tick < TPlusDelta)
				{
					ConsumerRecords[ThreadStateTicket.MyModuloAssignment].PerBufferBookmarks[WhichBuffer]++;
					if (CandidateRecord->count > 0 && ((CandidateRecord->hash) % this->CountOfConsumers) == ThreadStateTicket.MyModuloAssignment) //todo: mod zero happens here.
					{
						return RecordFetchState( WhichBuffer, CandidateRecord.value());
					}
				}
				else
				{
					break; // we are encroaching on THE FUTURE (we may be starting to read an unfinished tick, which can be real bad)
				}
			}
		}
		++WhichBuffer; //this is a ref
	}
	//reset, and effectively return EoF
	ConsumerRecords[ThreadStateTicket.MyModuloAssignment].BufferRingBookmark = 0;
	return RecordFetchState( -1, std::nullopt);
}

//todo: use the threadstateticket and boneticket to check what tick and "roll over" cycle the BLKRing thinks it's on.
//we can use this to make sure we're requesting a safe boneset that's of the right generation.
BLK::TransientQueuedDataRange BLK::BLKRing::GetBoneIterator(RecordFetchState& BoneTicket, WorkerStateBundle& ThreadStateTicket)
{
	if (BoneTicket.second.has_value())
	{
		auto& localref = BoneTicket.second.value();
		auto bindstart = ProducerBuffers[BoneTicket.first].Bones.get(localref.start);
		if (bindstart.has_value())
		{
			auto localbindref = &bindstart.value();
				_mm_prefetch(reinterpret_cast<char const*>(localbindref), _MM_HINT_T0);
			//TODO TODO TODO
			//THIS NEEDS TO NOT RETURN A POINTER OR YOU WILL WALK YOUR ASS OFF THE END OF THE ARRAY
			//ALSO NEED TO CHECK IF THIS HAPPENS WITH RECORDS? LIKE WHY IS MAX SO HIGH?!
				return TransientQueuedDataRange(localref.start, localref.start, BoneTicket.second.value().count, BoneTicket.first);
		}
	}
	return TransientQueuedDataRange(0, 0);
}

