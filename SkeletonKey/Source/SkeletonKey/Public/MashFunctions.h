#pragma once
#include "CoreMinimal.h"
#include "MashFunctions.generated.h"


//Mighty Micro Masher

USTRUCT()
struct SKELETONKEY_API FMMM
{
	GENERATED_BODY()
public:
	//we switched these from int to uint, which may actually not have been perfectly safe. if you see hash collision, that may be why
	//specifically, the hash may have derived some of its effective entropy from relying on the quirks of two's complement.
	/// A 64 bit hash function by Thomas Wang, Jan 1997
	/// See: http://web.archive.org/web/20071223173210/http://www.concentric.net/~Ttwang/tech/inthash.htm
	/// @param inValue Value to hash
	/// @return Hash
	static inline uint64 FastHash64(uint64 inValue);
	static inline uint32 FastHash32(uint32 key);
	static inline uint32 FastHash6432(uint64 key);

	static FString WhyDoIExist();
};


uint64 FMMM::FastHash64(uint64 inValue)
{
	uint64 hash = inValue;
	hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
	hash = hash ^ (hash >> 24);
	hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
	hash = hash ^ (hash >> 14);
	hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
	hash = hash ^ (hash >> 28);
	hash = hash + (hash << 31);
	return hash;
}

uint32 FMMM::FastHash32(uint32 key)
{
	key = ~key + (key << 15); // key = (key << 15) - key - 1;
	key = key ^ (key >> 12);
	key = key + (key << 2);
	key = key ^ (key >> 4);
	key = key * 2057; // key = (key + (key << 3)) + (key << 11);
	key = key ^ (key >> 16);
	return key;
}

uint32 FMMM::FastHash6432(uint64 key)
{
	key = (~key) + (key << 18); // key = (key << 18) - key - 1;
	key = key ^ (key >> 31);
	key = key * 21; // key = (key + (key << 2)) + (key << 4);
	key = key ^ (key >> 11);
	key = key + (key << 6);
	key = key ^ (key >> 22);
	return key;
}

//past here, these are drawn from hash inspector then borked.
inline uint8_t lowerbias328(uint32_t x)
{
	x ^= x >> 16;
	x *= 0xa812d533;
	x ^= x >> 15;
	x *= 0xb278e4ad;
	x ^= x >> 23;
	x *= 0xd533a813;
	x %= 256;
	return x;
}

inline uint16_t hash16_s6(uint16_t x)
{
	x += x << 7; x ^= x >> 8;
	x += x << 3; x ^= x >> 2;
	x += x << 4; x ^= x >> 8;
	return x;
}

inline uint8_t hash16_s6M8(uint16_t x)
{
	x += x << 7; x ^= x >> 8;
	x += x << 3; x ^= x >> 2;
	x += x << 4; x ^= x >> 8;
	return x % 255;
}

typedef FMMM MashFunctions;