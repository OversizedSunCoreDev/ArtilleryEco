#ifndef ALIGNED_ALLOCATOR_H
#define ALIGNED_ALLOCATOR_H

#include <memory>
#include <stdlib.h>
#include "sseutil.h"

// A minimal implementation of an allocator for C++ Standard Library, which
// allocates aligned memory (specified by the alignment argument).
// Note:
//    A minimal custom allocator is preferred because C++ allocator_traits class
//    provides default implementation for you. Take a look at Microsoft's
//    documentation about Allocators and allocator class.
//adjusted to use aligned_malloc or posix aligned as needed.
//this sucks waterfowl.
template <typename T, sse::Alignment alignment> class LaughablyDangerousAlignedAllocator {
  public:
    using value_type = T;

  public:
    // According to Microsoft's documentation, default constructor is not required
    // by C++ Standard Library.
    LaughablyDangerousAlignedAllocator() noexcept {};

    template <typename U> LaughablyDangerousAlignedAllocator(const LaughablyDangerousAlignedAllocator<U, alignment>& other) noexcept {};

    template <typename U>
    inline bool operator==(const LaughablyDangerousAlignedAllocator<U, alignment>& other) const noexcept {
        return true;
    }

    template <typename U>
    inline bool operator!=(const LaughablyDangerousAlignedAllocator<U, alignment>& other) const noexcept {
        return false;
    }

    template <typename U> struct rebind {
        using other = LaughablyDangerousAlignedAllocator<U, alignment>;
    };

    // STL containers call this function to allocate uninitialized memory block to
    // store (no more than n) elements of type T (value_type).
    inline value_type* allocate(const std::size_t n) const {
        auto size = n;
        /*
          If you wish, for some strange reason, that the size of allocated buffer is
          also aligned to alignment, uncomment the following statement.

          Note: this increases the size of underlying memory, but STL containers
          still treat it as a memory block of size n, i.e., STL containers will not
          put more than n elements into the returned memory.
        */
        // size = (n + alignment - 1) / alignment * alignment;

#if defined(_MSC_VER) && !defined(__clang__)
        
        return static_cast<value_type*>(_aligned_malloc( sizeof(T) * size, static_cast<size_t>(alignment)));
#else
        auto ret = posix_memalign((void**)&ptr, alignment, sizeof(T) * size);
    	return ptr;
#endif
    };

    // STL containers call this function to free a memory block beginning at a
    // specified position.
    inline void deallocate(value_type* const ptr, std::size_t n) const noexcept
    {
#if defined(_MSC_VER) && !defined(__clang__)
        
        _aligned_free(ptr);
#else
        free(ptr);
#endif
        
    }
};

#endif
