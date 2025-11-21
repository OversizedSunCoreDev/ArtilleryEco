#include "ConservedTagContainer.h"

bool FTagStateRepresentation::Find(uint16 Numerology)
{
	for (uint8 i = 0; i < FAST_TAG_MAX_C; i++)
	{
		if (Tags[i] == Numerology)
		{
			return true;
		}
	}
	return false;
}

bool FTagStateRepresentation::Remove(uint16 Numerology)
{
	bool foundandremoved = false;
	for (uint8 i = 0; i < FAST_TAG_MAX_C; i++)
	{
		if (Tags[i] == Numerology)
		{
			Tags[i] = 0;
			[[maybe_unused]] uint32_t A = snagged.fetch_and(0 << i, std::memory_order_acquire);
			foundandremoved = true;
		}
	}
	return foundandremoved;
}

bool FTagStateRepresentation::Add(uint16 Numerology)
{
	for (uint8 i = 0; i < FAST_TAG_MAX_C; i++)
	{
		//attempt weakly to prevent double entry. remove removes all instances, so this isn't a huge problem
		//we just want to make it only arise when contested in specific ways.
		if (Tags[i] == Numerology)
			return true;
	}
	for (uint8 i = 0; i < FAST_TAG_MAX_C; i++)
	{
		//this protects us from add races pretty well. now to get a double entry we need a fairly specific sequence
		//to all line up to contend.
		if (Tags[i] == 0 || Tags[i] == Numerology)
		{
			uint32_t A = snagged.fetch_and(1 << i, std::memory_order_acquire);
			if ((A & 1 << i) == 0) //was unset, is now set.
			{
				//we got it.
				Tags[i] = Numerology;
				return true;
			}
		}
	}

	return false;
}

void FConservedTagContainer::CacheLayer()
{
	uint32 index = CurrentHistory.GetNextIndex(CurrentWriteHead);
	CurrentHistory[index] = MakeShareable(new UnderlyingFTL());
	TSharedPtr<UnderlyingTagReverse> WornRing = DecoderRing.Pin();
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
			uint16_t Numerology = *search;
			return Tags->Find(Numerology);
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
			uint16_t Numerology = *search;
			return Tags->Remove(Numerology);
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
			uint16_t Numerology = *search;
			return Tags->Add(Numerology);
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