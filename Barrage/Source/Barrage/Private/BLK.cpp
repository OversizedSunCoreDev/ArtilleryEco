#include "BLK.h"
#include "MashFunctions.h"



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
		NewRecord.count = count;
		NewRecord.hash = hash16_s6M8(NewRecord.start);
		NewRecord.key = Key;
		ProducerBuffers[ThreadStateTicket.MyBufferAssignment].Records.Add(NewRecord, tick%256);
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
				if (CandidateRecord.value().tick == (UpToThisTick+1)% MaxFallBehind)
				{
					ConsumerRecords[ThreadStateTicket.MyModuloAssignment].PerBufferBookmarks[WhichBuffer]++;
					//DANGER WILL ROBINSON! This line is _risky_, as if consumers are mistakenly added during a tick instead of at the start of a tick,
					//the count for "consumers at start of tick" will not be updated in a safe way and this CAN lead to missed data in very rare cases.
					//more likely, it will cause double executions. Consumers must be added while consume phase is not taking place.
					if (CandidateRecord->count > 0 && (abs(CandidateRecord->hash) % this->CountOfConsumers) == ThreadStateTicket.MyModuloAssignment) //todo: mod zero happens here.
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

