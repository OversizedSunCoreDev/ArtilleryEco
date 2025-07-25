#pragma once
#include "Structures/flat_hash_map.hpp"
#include "Structures/PascalCircularBuffer.h"
#include "SkeletonTypes.h"
//This allocator relies on both an arena AND a pool. Each arena is bound to the TICK that created it. Each arena
//is retained as long as that tick remains relevant. this is SPECIFICALLY useful for things that have a dynamic memory
//load unsuitable for a strict linear allocator but that can be deallocated in a single operation at a known safe time.
//The name is not a joke. This is safe, but it's not simple.
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_START
#include "tlsf.h"

#include <memory>
THIRD_PARTY_INCLUDES_END

// This allocator is used in the following way. Allocate an arena block within a tick. Allocate using this
//allocator wrapper as normal. Within the duration of the tick, this acts as a normal pooled allocator built
//on the TLSF. You are responsible for the TrueLifecycleManager which is a shared pointer. This makes an ideal
//allocator for self-contained large objects that have highly variable internal structure.
//This is a BAD choice for a general purpose allocator. It is also probably a bad choice for objects with
//extremely regular internal structures.
//https://stackoverflow.com/questions/21028299/is-this-behavior-of-vectorresizesize-type-n-under-c11-and-boost-container/21028912#21028912
//for the... template magicka
template  <typename T>
class LOCOMOCORE_API IntraTickThreadblindAlloc;

// Specialization for `void`, but we no longer need to define `pointer` and `const_pointer`
template <>
class LOCOMOCORE_API IntraTickThreadblindAlloc<void> 
{
public:
	typedef void value_type;

	template <class U>
	struct rebind { typedef IntraTickThreadblindAlloc<U> other; };
};

// Define the custom stl_allocator inheriting from std::allocator
//if you have more time, I recommend instead building off https://en.cppreference.com/w/cpp/memory/polymorphic_allocator.html
template <typename T> //sane default
class LOCOMOCORE_API IntraTickThreadblindAlloc : public std::allocator<T> 
{
public:
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;
	typedef char pooled;
	using POOL = pooled;
	using TrueLifecycleManager = std::shared_ptr<char[]>;
	POOL* InitialPool = nullptr; //yeah I don't trust myself at all with memory and neither should you.
	tlsf_t UnderlyingTlsf;
	size_t Size;
	bool SPA; //by default, we are not a spa. we also do not have our Storage Pool Allocated

	// Default constructor
	//YOU MUST SET A SIZE. YOU MUST CALL INIT
	//this exists EXCLUSIVELY for compatibility reasons. To use it, you must set a size, then call init.
	//All of these allocators are basically shims for a pool allocator contained in a pool allocator.
	IntraTickThreadblindAlloc(): Size(0)
	{
		UnderlyingTlsf = nullptr;
		SPA = false;
	}

	//You must either call init or have a VERY good plan.
	//To use this safely, you should destroy the pool only AFTER
	//the tick it was used in.
	TrueLifecycleManager Init()
	{
		Size = std::max(256ull, Size);
		InitialPool =  new char[Size];
		memset(InitialPool, 0,   Size);//sandblast for the TLSF control structs.
		UnderlyingTlsf = tlsf_create_with_pool( InitialPool, Size);
		SPA = true;
		return std::make_shared<char[]>(*InitialPool);//what. i mean it makes.... sense....
	}

	//This is verboten. This is a bad idea. This is not a good plan.
	void DoNotUseThis()
	{
		//memset(InitialPool, 0,   Size);//sandblast for the TLSF control structs.
		UnderlyingTlsf = tlsf_create_with_pool( InitialPool, Size);
	}

	
	//if you MUST share a pool across allocator types, this can enable that.
	//This doesn't act as an owner. This. does. not. act. as. an. owner.
	IntraTickThreadblindAlloc(tlsf_t Safer, POOL* Ish, size_t Sized)
	{
		UnderlyingTlsf =	Safer;
		InitialPool =		Ish; //This doesn't act as an owner. This. does. not. act. as. an. owner.
		Size=Sized;
		SPA = true;
	}

	//we may be able to remove init now that we have a cloning constructor explicitly.
	IntraTickThreadblindAlloc(uint32_t StoragePoolSize) 
	{
		InitialPool = nullptr; 
		UnderlyingTlsf = nullptr;
		Size = StoragePoolSize;
		SPA = false;
	}
	
	~IntraTickThreadblindAlloc()
	{
		//management of the underlying pool is left to the user.
		(void)(InitialPool);
	};

	template <typename U>
	IntraTickThreadblindAlloc<T>(IntraTickThreadblindAlloc<U>& a)
	noexcept(std::is_nothrow_default_constructible<U>::value) : UnderlyingTlsf(a.UnderlyingTlsf), Size(a.Size),
	SPA(false)
	{
		if (a.InitialPool != nullptr)
		{
			InitialPool = a.InitialPool;
			SPA = true;
		}
		else
		{
			InitialPool = nullptr;
		}
	}
	
	// Copy constructor
	template <class U>
	explicit IntraTickThreadblindAlloc(const IntraTickThreadblindAlloc<U>& a) noexcept: UnderlyingTlsf(a.UnderlyingTlsf), Size(a.Size),
		SPA(false)
	{
		if (a.InitialPool != nullptr)
		{
			InitialPool = a.InitialPool;
			SPA = true;
		}
		else
		{
			InitialPool = nullptr;
		}
	}


	// Rebind struct
	template <class U>
	struct rebind { typedef IntraTickThreadblindAlloc<U> other; };

	// Override allocate method to use custom allocation function
	pointer allocate(size_type n, typename std::allocator_traits<IntraTickThreadblindAlloc<void>>::const_pointer hint = 0) 
	{
		return static_cast<pointer>(tlsf_malloc(UnderlyingTlsf, n * sizeof(T)));
	}

	// Override deallocate method to use custom deallocation function
	void deallocate(pointer p, size_type n) 
	{
		tlsf_free(UnderlyingTlsf, p);
	}

	
		
	// You can inherit other methods like construct and destroy from std::allocator
};



// Comparison operators for compatibility
template <typename T, typename U>
inline bool operator==(const IntraTickThreadblindAlloc<T>&, const IntraTickThreadblindAlloc<U>) { return true; }

template <typename T, typename U>
inline bool operator!=(const IntraTickThreadblindAlloc<T>&, const IntraTickThreadblindAlloc<U>) { return false; }

template <typename T, typename A=IntraTickThreadblindAlloc<T>>
class DefaultAwareIntraTickAlloc : public A {
	typedef std::allocator_traits<A> a_t;
public:
	template <typename U> struct rebind {
		using other =
		  DefaultAwareIntraTickAlloc<
			U, typename a_t::template rebind_alloc<U>
		  >;
	};

	using A::A;
	

	
	template <typename U>
	void construct(U* ptr)
	  noexcept(std::is_nothrow_default_constructible<U>::value) {
		::new(static_cast<void*>(ptr)) U;
	}
	template <typename U, typename...Args>
	void construct(U* ptr, Args&&... args) {
		a_t::construct(static_cast<A&>(*this),
					   ptr, std::forward<Args>(args)...);
	}
};

//THIS ALLOCATOR IS NOT FULLY GENERAL. GOOD LUCK.
template class IntraTickThreadblindAlloc<uint32_t>;

using FStampedKeyPair = std::pair<uint32_t, FSkeletonKey>;
template class IntraTickThreadblindAlloc<uint64_t>;
template class IntraTickThreadblindAlloc<std::vector<uint32_t, IntraTickThreadblindAlloc<uint32_t>>>;
template class IntraTickThreadblindAlloc<std::pair<const uint32_t, uint32_t>>;
template class IntraTickThreadblindAlloc<std::pair<uint32_t, uint32_t>>;

template class DefaultAwareIntraTickAlloc<uint32_t>; //this is ONLY possible due to the joys of SFINAE.
template class DefaultAwareIntraTickAlloc<uint64_t>;
template class DefaultAwareIntraTickAlloc<FPascally_15>;
template class IntraTickThreadblindAlloc<FPascally_15>;

template class DefaultAwareIntraTickAlloc<FPascally_31>;
template class IntraTickThreadblindAlloc<FPascally_31>;
template class IntraTickThreadblindAlloc<std::vector<FPascally_15, IntraTickThreadblindAlloc<FPascally_15>>>;
template class DefaultAwareIntraTickAlloc<std::vector<FPascally_15, IntraTickThreadblindAlloc<FPascally_15>>>;

template class IntraTickThreadblindAlloc<std::vector<FSkeletonKey, IntraTickThreadblindAlloc<FSkeletonKey>>>;
template class DefaultAwareIntraTickAlloc<std::vector<FSkeletonKey, IntraTickThreadblindAlloc<FSkeletonKey>>>;
template class IntraTickThreadblindAlloc<std::vector<FStampedKeyPair, IntraTickThreadblindAlloc<FStampedKeyPair>>>;
template class DefaultAwareIntraTickAlloc<std::vector<FStampedKeyPair, IntraTickThreadblindAlloc<FStampedKeyPair>>>;
using DATAlloc_StampedKeyVector = DefaultAwareIntraTickAlloc<std::vector<FStampedKeyPair, IntraTickThreadblindAlloc<FStampedKeyPair>>>;


template class IntraTickThreadblindAlloc<std::vector<FPascally_31, IntraTickThreadblindAlloc<FPascally_31>>>;
template class DefaultAwareIntraTickAlloc<std::vector<FPascally_31, IntraTickThreadblindAlloc<FPascally_31>>>;
template class DefaultAwareIntraTickAlloc<std::pair<uint32_t, uint32_t>>;
template class IntraTickThreadblindAlloc<std::vector<uint32_t, DefaultAwareIntraTickAlloc<uint32_t>>>;
template class IntraTickThreadblindAlloc<struct ska::detailv3::sherwood_v3_entry<struct std::pair<unsigned int,unsigned int> > >;
template class DefaultAwareIntraTickAlloc<struct ska::detailv3::sherwood_v3_entry<struct std::pair<unsigned int,unsigned int> > >;
PRAGMA_POP_PLATFORM_DEFAULT_PACKING