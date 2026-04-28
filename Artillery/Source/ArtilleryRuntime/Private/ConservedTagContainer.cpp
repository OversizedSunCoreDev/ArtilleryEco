#include "ConservedTagContainer.h"

#include "LowLogTimeAndRate.h"

bool FTagStateRepresentation::Find(uint16 InternalCompressedTagCode)
{
	for (uint8 i = 0; i < FAST_TAG_MAX_C; i++)
	{
		if (Tags[i] == InternalCompressedTagCode)
		{
			return true;
		}
	}
	return false;
}

bool FTagStateRepresentation::Remove(uint16 InternalCompressedTagCode)
{
	bool foundandremoved = false;
	for (uint8 i = 0; i < FAST_TAG_MAX_C; i++)
	{
		if (Tags[i] == InternalCompressedTagCode)
		{
			Tags[i] = 0;
			[[maybe_unused]] uint32_t A = snagged.fetch_and(0 << i, std::memory_order_acquire);
			foundandremoved = true;
		}
	}
	return foundandremoved;
}

bool FTagStateRepresentation::Add(uint16 InternalCompressedTagCode)
{
	for (uint8 i = 0; i < FAST_TAG_MAX_C; i++)
	{
		//attempt weakly to prevent double entry. remove removes all instances, so this isn't a huge problem
		//we just want to make it only arise when contested in specific ways.
		if (Tags[i] == InternalCompressedTagCode)
			return true;
	}
	for (uint8 i = 0; i < FAST_TAG_MAX_C; i++)
	{
		//this protects us from add races pretty well. now to get a double entry we need a fairly specific sequence
		//to all line up to contend.
		if (Tags[i] == 0 || Tags[i] == InternalCompressedTagCode)
		{
			uint32_t A = snagged.fetch_and(1 << i, std::memory_order_acquire);
			if ((A & 1 << i) == 0) //was unset, is now set.
			{
				//we got it.
				Tags[i] = InternalCompressedTagCode;
				return true;
			}
		}
	}

	return false;
}

void FConservedTagContainer::CacheLayer()
{
	uint32 index = CurrentHistory.GetNextIndex(CurrentWriteHead);
	if (CurrentHistory[index] != nullptr)
	{
		CurrentHistory[index]->SetNumUninitialized(0, EAllowShrinking::No);
	}
	else
	{
		CurrentHistory[index] = MakeShareable(new UnderlyingFTL());
	}
	TSharedPtr<UnderlyingTagReverse> WornRing = DecoderRing.Pin();
	//TODO: rework this fever dream. profiling indicates an issue somewhere in here.
	//while a linear pass is fast in many cases, to be honest, I suspect we're spending a lot of time here
	//enough that it's worth considering something faster like a flathash. a traditional hash is likely too slow on such a small set
	//but I wonder if we even need to do this at all.
	if (WornRing && Tags && Tags->Tags)
	{
		for (uint16_t tagcode : Tags->Tags)
		{
			FGameplayTag* ATag = WornRing->Find(tagcode);
			if (ATag != nullptr)
			{
				CurrentHistory[index]->Add(*ATag);
			}
		}
		++CurrentWriteHead;
	}
}

FConservedTags FConservedTagContainer::GetReference()
{
	if (AccessRefController.IsValid())
	{
		TSharedPtr<FConservedTagContainer> scopeguard = AccessRefController.Pin();
		return scopeguard;
	}
	return nullptr;
}

//frame numbering starts at _1_
FTagLayer FConservedTagContainer::GetFrameByNumber(uint64_t FrameNumber)
{
	return CurrentHistory[CurrentHistory.GetNextIndex(FrameNumber-1)];
}

bool FConservedTagContainer::Find(FGameplayTag Bot)
{
	if (Tags)
	{
		if (uint16_t* search = SeenT->Find(Bot); search != nullptr)
		{
			uint16_t InternalCompressedTagCode = *search;
			return Tags->Find(InternalCompressedTagCode);
		}
	}
	return false;
}

bool FConservedTagContainer::Remove(FGameplayTag Bot)
{
	if (Tags)
	{
		if (uint16_t* search = SeenT->Find(Bot); search != nullptr)
		{
			uint16_t InternalCompressedTagCode = *search;
			return Tags->Remove(InternalCompressedTagCode);
		}
	}
	return false;
}

bool FConservedTagContainer::Add(FGameplayTag Bot)
{
	if (Tags)
	{
		if (uint16_t* search = SeenT->Find(Bot); search != nullptr)
		{
			uint16_t InternalCompressedTagCode = *search;
			return Tags->Add(InternalCompressedTagCode);
		}
	}
	return false;
}

//todo: doublecheck my math here.
TSharedPtr<TArray<FGameplayTag>> FConservedTagContainer::GetAllTags()
{
	return CurrentHistory[CurrentHistory.GetPreviousIndex(CurrentWriteHead)];
}

//todo: again doublecheck my math here. can peek the FConservedAttrib. ATM, I gotta get this wired up.
TSharedPtr<TArray<FGameplayTag>> FConservedTagContainer::GetAllTags(uint64_t FrameNumber)
{
	return CurrentHistory[CurrentHistory.GetPreviousIndex(FrameNumber)];
}