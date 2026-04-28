#include "AtomicTagArray.h"


bool RecordTags()
{
	return true;
}

AtomicTagArray::AtomicTagArray()
{
	FastEntities = MakeShareable(new Entities());
}

void AtomicTagArray::Init()
{
	SeenT = MakeShareable(new UnderlyingTagMapping());
	MasterDecoderRing = MakeShareable(new UnderlyingTagReverse());
	FGameplayTagContainer Container;
	UGameplayTagsManager::Get().RequestAllGameplayTags(Container, false);
	TArray<FGameplayTag> TagArray;
	Container.GetGameplayTagArray(TagArray);
	for (FGameplayTag& Tag : TagArray)
	{
		SeenT->Emplace(Tag, ++Counter);
		MasterDecoderRing->Emplace(Counter,Tag);
	}
}

bool AtomicTagArray::Add(FSkeletonKey Top, FGameplayTag Bot)
{
	uint32_t Key = KeyToHash(Top);
	if (!AddImpl(Key, Bot))
	{
		return false; // we really should do more but right now, this is just for entities with seven tags or less.
		//I had a sketch of something that was a little more versatile, but ultimately, I actually think it might be
		//overkill. entities should EITHER get added to FastSKRP or to the normal tag set.
	}
	return true;
	//okay this is a satanic mess.
}

//it is STRONGLY advised that you NEVER call this directly.
//please instead use RegisterGameplayTags or DeregisterGameplayTags
FConservedTags AtomicTagArray::NewTagContainer(FSkeletonKey Top)
{
	uint32_t Key = KeyToHash(Top);
	TSharedPtr<Entities> HOpen = FastEntities;
	if (FTagsPtr Tags; HOpen && HOpen->visit(Key, [&Tags](auto& a) { Tags = a.second; }) == 0)
	{
		Tags = MakeShareable<FTagStateRepresentation>(new FTagStateRepresentation());
		FConservedTags That = MakeShareable(new FConservedTagContainer(Tags, MasterDecoderRing, SeenT));
		That->AccessRefController =  That.ToWeakPtr();
		Tags->AccessRefController = That->AccessRefController;
		HOpen->insert_or_assign(Key, Tags);
		return That; //this begins a chain of events that leads to everything getting reference counted correctly.
	}
	return nullptr; // this is a lot tidier but it's still a horror.
}

uint32_t AtomicTagArray::KeyToHash(FSkeletonKey Top)
{
	uint64_t TypeInfo = GET_SK_TYPE(Top);
	uint64_t KeyInstance = Top & SKELLY::SFIX_HASH_EXT;
	return HashCombineFast(TypeInfo, KeyInstance);
}

bool AtomicTagArray::Find(FSkeletonKey Top, FGameplayTag Bot)
{
	uint32_t Key = KeyToHash(Top);
	TSharedPtr<Entities> HOpen = FastEntities;
	if (uint16_t* search = SeenT->Find(Bot); HOpen && search != nullptr)
	{
		uint16_t InternalCompressedTagCode = *search;
		FTagsPtr Tags;
		return !HOpen->visit(Key, [&Tags](auto& a) { Tags = a.second; }) ? false : Tags->Find(InternalCompressedTagCode);
	}
	return false;
}

bool AtomicTagArray::Erase(FSkeletonKey Top)
{
	uint32_t Key = KeyToHash(Top);
	TSharedPtr<Entities> HOpen = FastEntities;
	return HOpen ?  (bool)(HOpen->erase(Key)) : true;
}

// ReSharper disable once CppMemberFunctionMayBeConst
//returns true if the tag is not a real tag OR if the owning entity was removed OR the tag was removed 
//returns false if tag and entity are live but no tag was removed.
bool AtomicTagArray::Remove(FSkeletonKey Top, FGameplayTag Bot)
{
	uint32_t Key = KeyToHash(Top);
	TSharedPtr<Entities> HOpen = FastEntities;
	if (uint16_t* search = SeenT->Find(Bot); search != nullptr)
	{
		FTagsPtr Tags;
		uint16_t InternalCompressedTagCode = *search;
		if (HOpen && !HOpen->visit(Key, [&Tags](auto& a) { Tags = a.second; }))
		{
			return true;
		}
		Tags->Remove(InternalCompressedTagCode);
	}
	return false;
}

bool AtomicTagArray::SkeletonKeyExists(FSkeletonKey Top)
{
	uint32_t Key = KeyToHash(Top);
	TSharedPtr<Entities> HOpen = FastEntities;
	return HOpen ? HOpen->contains(Key) : false;
}


inline FConservedTags AtomicTagArray::GetReference(FSkeletonKey Top)
{
	FTagsPtr into = nullptr;
	uint32_t Key = KeyToHash(Top);
	TSharedPtr<Entities> HOpen = FastEntities;
	return HOpen && HOpen->visit(Key, [&into](auto& a) { into = a.second; }) && into.IsValid() ? into->AccessRefController.Pin() : nullptr;
}

bool AtomicTagArray::Empty()
{
	TSharedPtr<Entities> HOpen = FastEntities;
	FastEntities.Reset();
	SeenT.Reset(); // let it go, but don't blast it.
	MasterDecoderRing.Reset();
	return true;
}

bool AtomicTagArray::AddImpl(uint32_t Key, FGameplayTag Bot)
{
	TSharedPtr<Entities> HOpen = FastEntities;
	if (uint16_t* search = SeenT->Find(Bot); HOpen && search != nullptr)
	{
		FTagsPtr Tags;
		uint16_t InternalCompressedTagCode = *search;
		 if (HOpen->visit(Key, [&Tags](auto& a) { Tags = a.second; }) && Tags && Tags.IsValid())
		 {
		 	Tags->Add(InternalCompressedTagCode);
		 	return true;
		 }
		 else
		 {
		 	return false;
		 }
	}
	return false;
}
