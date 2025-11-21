#pragma once
#ifndef LORDLY_SKELETON_H
#define LORDLY_SKELETON_H
#define LORDLY_SKELETON_VER 0

namespace SKELLY
{///											  4bits for type, 28 for read only meta, 32 for hash.
	// One hex digit is one nibble, so our binary pattern is this:
	//							[TTTT][MMMM MMMM MMMM MMMM MMMM MMMM MMMM][HHHH HHHH HHHH HHHH HHHH HHHH HHHH HHHH]
	constexpr static uint64_t	SFIX_MASK_OUT = 0x0FFFFFFFFFFFFFFF;
	constexpr static uint64_t	SFIX_MASK_EXT = 0xF000000000000000;
	constexpr static uint64_t	SFIX_HASH_EXT = 0x00000000FFFFFFFF;
	constexpr static uint64_t	SFIX_META_EXT = 0x0FFFFFFF00000000;
	constexpr static uint64_t	SFIX_NONE	  = 0x0000000000000000;
	constexpr static uint64_t	SFIX_KEYTOMETA= 0xFFFFFFFF0FFFFFFF; // this puts a notch in the hash so that it can be used as meta value.
	constexpr static uint64_t	SFIX_ACT_COMP =	SFIX_MASK_EXT; //special case, since this is the basal capability.
	//An archetype or shared instance gets one of these...
	constexpr static uint64_t	SFIX_ART_GUNS = 0x1000000000000000;
	//An individual _instance_ gets one of these...
	constexpr static uint64_t	SFIX_ART_1GUN = 0x2000000000000000;
	//BOOOLETS
	constexpr static uint64_t	SFIX_GUN_SHOT = 0x3000000000000000;
	constexpr static uint64_t	SFIX_BAR_PRIM = 0x4000000000000000;
	constexpr static uint64_t	SFIX_ART_ACTS = 0x5000000000000000;
	constexpr static uint64_t	SFIX_ART_FCMS = 0x6000000000000000;
	constexpr static uint64_t	SFIX_ART_FACT = 0x7000000000000000;
	constexpr static uint64_t	SFIX_PLAYERID = 0x8000000000000000;
	//HIGHER GAMEPLAY CONSTRUCTS, THIS TEXT IS FOR YOU ALONE.
	constexpr static uint64_t	SFIX_BONEKEY =  0x9000000000000000;
	//mass interop key. used internally at the moment
	constexpr static uint64_t	SFIX_MASSIDP =  0xA000000000000000;
	constexpr static uint64_t	SFIX_STELLAR =  0xB000000000000000;
	constexpr static uint64_t	SFIX_UNUSEDC =  0xC000000000000000;
	constexpr static uint64_t	SFIX_UNUSEDD =  0xD000000000000000;
	constexpr static uint64_t	SFIX_UNUSEDE =  0xE000000000000000;
	constexpr static uint64_t	SFIX_SK_LORD =  0xF000000000000000;
}
	static inline bool IS_OF_SK_TYPE(uint64_t MY_HASH,uint64_t MY_MASK) {return (MY_HASH & SKELLY::SFIX_MASK_EXT) == MY_MASK;};
	static inline uint64_t GET_SK_TYPE(uint64_t MY_HASH) {return (MY_HASH & SKELLY::SFIX_MASK_EXT);};
	static inline uint64_t FORGE_SKELETON_KEY(uint64_t MY_HASH,uint64_t MY_MASK) {return (MY_HASH & SKELLY::SFIX_MASK_OUT) | MY_MASK;};
	constexpr uint64_t BoneKey_Infix = SKELLY::SFIX_BONEKEY;
	constexpr uint64_t GunInstance_Infix = SKELLY::SFIX_ART_1GUN;

static inline uint64_t FORGE_DEPENDENT_SKELETON_KEY(uint64_t parent, uint32_t localunique,
                                                    uint64_t MY_MASK = SKELLY::SFIX_ART_FACT) //by default dependent keys are facts.
{
	auto ret = parent & SKELLY::SFIX_KEYTOMETA;
	ret = (ret << 32) | localunique;
	ret = FORGE_SKELETON_KEY(ret, SKELLY::SFIX_ART_FACT); // I forgot this and lost rather a lot of time.
	return ret;
};
#endif

//TODO: replace with and standardize on fast hash if needed.
#define MAKE_BONEKEY(turn_into_key) FBoneKey(PointerHash(turn_into_key))
#define MAKE_ACTORKEY(turn_into_key) ActorKey(PointerHash(turn_into_key))

#define MYSTIC_STANDARDIZED_OFFSET 17
