#pragma once
#include <iostream>

#include "AtypicalDistances.h"
#include <bitset>
#include <bit>

#include "LibMorton/morton.h"
#include "Quaternion.h"

class FZOrderDistances
{
public:

	static inline const FRotator3d TurnwiseTrick = FRotator3d(0.1,0.1,0.1);
	//interleave  
	static uint64_t ZInterleave(uint32 x, uint32 y, uint32 z)
	{
		return (x << 2) + (y << 1) + z;
	}

	//we're going to slice bits off the exponent, but maybe not how you expect.
	//1000 0001 is the exp for 4. We'll be using 0b101111100 as our discriminant.
	static inline uint32 LIMITS(float f)
	{
		auto a = std::bit_cast<uint32_t, float>(f*512);
		return f <= -8 || f >= 8 ? a : 0;
	}

	//morton\z interleave expansion for 8 bits, right justified.
	static constexpr uint32_t NormalLUT[256] =
	{
		//0000000000000000
		0b00000000000000000000000000000000, //0
		0b00000000000000000000000000000100,
		0b00000000000000000000000000100000, //2
		0b00000000000000000000000000100100,
		0b00000000000000000000000100000000, //4
		0b00000000000000000000000100000100,
		0b00000000000000000000000100100000, //6
		0b00000000000000000000000100100100,
		0b00000000000000000000100000000000, //8
		0b00000000000000000000100000000100,
		0b00000000000000000000100000100000, //10
		0b00000000000000000000100000100100,
		0b00000000000000000000100100000000, //12
		0b00000000000000000000100100000100,
		0b00000000000000000000100100100000, //14
		0b00000000000000000000100100100100,
		0b00000000000000000100000000000000, //16
		0b00000000000000000100000000000100, //17
		0b00000000000000000100000000100000, //18
		0b00000000000000000100000000100100, //19
		0b00000000000000000100000100000000, //20
		0b00000000000000000100000100000100, //21
		0b00000000000000000100000100100000, //22
		0b00000000000000000100000100100100, //23
		0b00000000000000000100100000000000, //24
		0b00000000000000000100100000000100, //25
		0b00000000000000000100100000100000, //26
		0b00000000000000000100100000100100, //27
		0b00000000000000000100100100000000, //28
		0b00000000000000000100100100000100, //29
		0b00000000000000000100100100100000, //30
		0b00000000000000000100100100100100, //31
		0b00000000000000100000000000000000,
		0b00000000000000100000000000000100,
		0b00000000000000100000000000100000,
		0b00000000000000100000000000100100,
		0b00000000000000100000000100000000,
		0b00000000000000100000000100000100,
		0b00000000000000100000000100100000,
		0b00000000000000100000000100100100, //8 rows
		0b00000000000000100000100000000000,
		0b00000000000000100000100000000100,
		0b00000000000000100000100000100000,
		0b00000000000000100000100000100100,
		0b00000000000000100000100100000000,
		0b00000000000000100000100100000100,
		0b00000000000000100000100100100000,
		0b00000000000000100000100100100100, //8 rows
		0b00000000000000100100000000000000,
		0b00000000000000100100000000000100,
		0b00000000000000100100000000100000,
		0b00000000000000100100000000100100,
		0b00000000000000100100000100000000,
		0b00000000000000100100000100000100,
		0b00000000000000100100000100100000,
		0b00000000000000100100000100100100, //8 rows
		0b00000000000000100100100000000000,
		0b00000000000000100100100000000100,
		0b00000000000000100100100000100000,
		0b00000000000000100100100000100100,
		0b00000000000000100100100100000000,
		0b00000000000000100100100100000100,
		0b00000000000000100100100100100000,
		0b00000000000000100100100100100100,
		0b00000000000100000000000000000000,
		0b00000000000100000000000000000000,
		0b00000000000100000000000000000100,
		0b00000000000100000000000000100000,
		0b00000000000100000000000000100100,
		0b00000000000100000000000100000000,
		0b00000000000100000000000100000100,
		0b00000000000100000000000100100000,
		0b00000000000100000000000100100100,
		0b00000000000100000000100000000000,
		0b00000000000100000000100000000100,
		0b00000000000100000000100000100000,
		0b00000000000100000000100000100100,
		0b00000000000100000000100100000000,
		0b00000000000100000000100100000100,
		0b00000000000100000000100100100000,
		0b00000000000100000000100100100100,
		0b00000000000100000100000000000000,
		0b00000000000100000100000000000100,
		0b00000000000100000100000000100000,
		0b00000000000100000100000000100100,
		0b00000000000100000100000100000000,
		0b00000000000100000100000100000100,
		0b00000000000100000100000100100000,
		0b00000000000100000100000100100100,
		0b00000000000100000100100000000000,
		0b00000000000100000100100000000100,
		0b00000000000100000100100000100000,
		0b00000000000100000100100000100100,
		0b00000000000100000100100100000000,
		0b00000000000100000100100100000100,
		0b00000000000100000100100100100000,
		0b00000000000100000100100100100100,
		0b00000000000100100000000000000000,
		0b00000000000100100000000000000100,
		0b00000000000100100000000000100000,
		0b00000000000100100000000000100100,
		0b00000000000100100000000100000000,
		0b00000000000100100000000100000100,
		0b00000000000100100000000100100000,
		0b00000000000100100000000100100100,
		0b00000000000100100000100000000000,
		0b00000000000100100000100000000100,
		0b00000000000100100000100000100000,
		0b00000000000100100000100000100100,
		0b00000000000100100000100100000000,
		0b00000000000100100000100100000100,
		0b00000000000100100000100100100000,
		0b00000000000100100000100100100100,
		0b00000000000100100100000000000000,
		0b00000000000100100100000000000100,
		0b00000000000100100100000000100000,
		0b00000000000100100100000000100100,
		0b00000000000100100100000100000000,
		0b00000000000100100100000100000100,
		0b00000000000100100100000100100000,
		0b00000000000100100100000100100100,
		0b00000000000100100100100000000000,
		0b00000000000100100100100000000100,
		0b00000000000100100100100000100000,
		0b00000000000100100100100000100100,
		0b00000000000100100100100100000000,
		0b00000000000100100100100100000100,
		0b00000000000100100100100100100000,
		0b00000000000100100100100100100100, // seven set bits. 21 used.
		//high byte will stop at this break line.
		0b00000000100000000000000000000000,
		0b00000000100000000000000000000100,
		0b00000000100000000000000000100000,
		0b00000000100000000000000000100100,
		0b00000000100000000000000100000000,
		0b00000000100000000000000100000100,
		0b00000000100000000000000100100000,
		0b00000000100000000000000100100100,
		0b00000000100000000000100000000000,
		0b00000000100000000000100000000100,
		0b00000000100000000000100000100000,
		0b00000000100000000000100000100100,
		0b00000000100000000000100100000000,
		0b00000000100000000000100100000100,
		0b00000000100000000000100100100000,
		0b00000000100000000000100100100100,
		0b00000000100000000100000000000000,
		0b00000000100000000100000000000100,
		0b00000000100000000100000000100000,
		0b00000000100000000100000000100100,
		0b00000000100000000100000100000000,
		0b00000000100000000100000100000100,
		0b00000000100000000100000100100000,
		0b00000000100000000100000100100100,
		0b00000000100000000100100000000000,
		0b00000000100000000100100000000100,
		0b00000000100000000100100000100000,
		0b00000000100000000100100000100100,
		0b00000000100000000100100100000000,
		0b00000000100000000100100100000100,
		0b00000000100000000100100100100000,
		0b00000000100000000100100100100100,
		0b00000000100000100000000000000000,
		0b00000000100000100000000000000100,
		0b00000000100000100000000000100000,
		0b00000000100000100000000000100100,
		0b00000000100000100000000100000000,
		0b00000000100000100000000100000100,
		0b00000000100000100000000100100000,
		0b00000000100000100000000100100100,
		0b00000000100000100000100000000000,
		0b00000000100000100000100000000100,
		0b00000000100000100000100000100000,
		0b00000000100000100000100000100100,
		0b00000000100000100000100100000000,
		0b00000000100000100000100100000100,
		0b00000000100000100000100100100000,
		0b00000000100000100000100100100100,
		0b00000000100000100100000000000000,
		0b00000000100000100100000000000100,
		0b00000000100000100100000000100000,
		0b00000000100000100100000000100100,
		0b00000000100000100100000100000000,
		0b00000000100000100100000100000100,
		0b00000000100000100100000100100000,
		0b00000000100000100100000100100100,
		0b00000000100000100100100000000000,
		0b00000000100000100100100000000100,
		0b00000000100000100100100000100000,
		0b00000000100000100100100000100100,
		0b00000000100000100100100100000000,
		0b00000000100000100100100100000100,
		0b00000000100000100100100100100000,
		0b00000000100000100100100100100100,
		0b00000000100100000000000000000000,
		0b00000000100100000000000000000100,
		0b00000000100100000000000000100000,
		0b00000000100100000000000000100100,
		0b00000000100100000000000100000000,
		0b00000000100100000000000100000100,
		0b00000000100100000000000100100000,
		0b00000000100100000000000100100100,
		0b00000000100100000000100000000000,
		0b00000000100100000000100000000100,
		0b00000000100100000000100000100000,
		0b00000000100100000000100000100100,
		0b00000000100100000000100100000000,
		0b00000000100100000000100100000100,
		0b00000000100100000000100100100000,
		0b00000000100100000000100100100100,
		0b00000000100100000100000000000000,
		0b00000000100100000100000000000100,
		0b00000000100100000100000000100000,
		0b00000000100100000100000000100100,
		0b00000000100100000100000100000000,
		0b00000000100100000100000100000100,
		0b00000000100100000100000100100000,
		0b00000000100100000100000100100100,
		0b00000000100100000100100000000000,
		0b00000000100100000100100000000100,
		0b00000000100100000100100000100000,
		0b00000000100100000100100000100100,
		0b00000000100100000100100100000000,
		0b00000000100100000100100100000100,
		0b00000000100100000100100100100000,
		0b00000000100100000100100100100100,
		0b00000000100100100000000000000000,
		0b00000000100100100000000000000100,
		0b00000000100100100000000000100000,
		0b00000000100100100000000000100100,
		0b00000000100100100000000100000000,
		0b00000000100100100000000100000100,
		0b00000000100100100000000100100000,
		0b00000000100100100000000100100100,
		0b00000000100100100000100000000000,
		0b00000000100100100000100000000100,
		0b00000000100100100000100000100000,
		0b00000000100100100000100000100100,
		0b00000000100100100000100100000000,
		0b00000000100100100000100100000100,
		0b00000000100100100000100100100000,
		0b00000000100100100000100100100100,
		0b00000000100100100100000000000000,
		0b00000000100100100100000000000100,
		0b00000000100100100100000000100000,
		0b00000000100100100100000000100100,
		0b00000000100100100100000100000000,
		0b00000000100100100100000100000100,
		0b00000000100100100100000100100000,
		0b00000000100100100100000100100100,
		0b00000000100100100100100000000000,
		0b00000000100100100100100000000100,
		0b00000000100100100100100000100000,
		0b00000000100100100100100000100100,
		0b00000000100100100100100100000100,
		0b00000000100100100100100100100000,
		0b00000000100100100100100100100100,
	};
	//THIS0000000000000000 SUCKS
	//this is a special LUT that turns 0b0V0V VVVV into 0b0V00 V00 V00V 00V0 0V00
	//Where V is the acknowledged set bits.
	static constexpr uint16_t SpecializedScatterLUT[128] =
	{
		//0000000000000000
		0b0000000000000000, //0
		0b0000000000000001,
		0b0000000000001000, //2
		0b0000000000001001,
		0b0000000001000000, //4
		0b0000000001000001,
		0b0000000001001000, //6
		0b0000000001001001,
		0b0000001000000000, //8
		0b0000001000000001,
		0b0000001000001000, //10
		0b0000001000001001,
		0b0000001001000000, //12
		0b0000001001000001,
		0b0000001001001000, //14
		0b0000001001001001,
		0b0001000000000000, //16
		0b0001000000000001, //17
		0b0001000000001000, //18
		0b0001000000001001, //19
		0b0001000001000000, //20
		0b0001000001000001, //21
		0b0001000001001000, //22
		0b0001000001001001, //23
		0b0001001000000000, //24
		0b0001001000000001, //25
		0b0001001000001000, //26
		0b0001001000001001, //27
		0b0001001001000000, //28
		0b0001001001000001, //29
		0b0001001001001000, //30
		0b0001001001001001, //31
		/*sure as shit seems like
	****0b1000000000000000,
		*should be next, right?
		*naw. 
		*/
		0xDEAD, //32's a breakpoint. that would mean that our 6th bit is set.
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD, //which you can see from our V0VVVVV pattern
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD, //is an invalid range. it turns out that compressing that bit is a huge pain
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD, //so I made the lookup table as a test for chatGPT's o4 mini model.
		0xDEAD,
		0xDEAD, //by which I mean, I tried to get it to make it for almost an hour.
		0xDEAD,
		0xDEAD, //then just zoned and did it myself in 25 minutes.
		0xDEAD, //The ones it generated were... horribly wrong. 
		0xDEAD,
		0xDEAD,
		//64 is our first "normalish" number again, but don't worry, shit goes bad here too...
		0b1000000000000000,
		0b1000000000000001,
		0b1000000000001000,
		0b1000000000001001,
		0b1000000001000000,
		0b1000000001000001,
		0b1000000001001000,
		0b1000000001001001, //8 rows
		0b1000001000000000,
		0b1000001000000001,
		0b1000001000001000,
		0b1000001000001001,
		0b1000001001000000,
		0b1000001001000001,
		0b1000001001001000,
		0b1000001001001001, //8 rows
		0b1001000000000000,
		0b1001000000000001,
		0b1001000000001000,
		0b1001000000001001,
		0b1001000001000000,
		0b1001000001000001,
		0b1001000001001000,
		0b1001000001001001, //8 rows
		0b1001001000000000,
		0b1001001000000001,
		0b1001001000001000,
		0b1001001000001001,
		0b1001001001000000,
		0b1001001001000001,
		0b1001001001001000,
		0b1001001001001001,
		//This feels wrong, doesn't it? It really felt wrong to me.
		//And the very top of our range (0b1100000 to 0b1111111 is also cooked)
		//I've tested this like forty times now. I'm p sure it's okay. ish.
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD, //which you can AGAIN see from our V0V VVVV pattern
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD,
		0xDEAD, //I really hated this.
		0xDEAD,
		0xDEAD, //I was pretty desperate to get an AI to generate this table.
		0xDEAD,
		0xDEAD, //I guess we still get to do the very worst tasks...
		0xDEAD, //cool.
		0xDEAD,
		0xDEAD
	};
	//this can be made FAR faster with intrinsics or better bitmath.
	//if it's ever the slow part, take a look at the 128 bit intrinsics.
	static inline uint64_t FloatToVoxelBits(float f)
	{
		auto bits = AtypicalDistances::FloatFlip(LIMITS(f));
		uint32 mbitsA = (bits & 0x0000FF00) >> 8;
		//remove the trailing 8 bits and top 8. 7-> 0111, leaving us with 15 bits of mantissa...
		uint32 mbitsB = (bits & 0x007F0000) >> 16; //break it into two bytes...
		//which sucks. a lot. but it's far more precision than a half float, in a sense. 5 bits of exp, 1 sign bit, 15 mantissa.

		//top 8 bits, masked like: 0b1011 1111. then shifted 25. leaving us 0b0101 1111 (9-> 1001, so B-> 1011)
		uint64 begin_compose = uint64(SpecializedScatterLUT[(bits & 0xBF000000) >> 25]) << 45; //18 bits...
		//std::cerr << "show bits: " <<  std::bitset<64>{ begin_compose | (uint64(NormalLUT[mbitsB]) << 24) | uint64(NormalLUT[mbitsA])} << std::endl;
		//the scatter LUT we're using is... odd.
		//we've turned 0b1_2 3456_ into 0b1002 0030 0400 5006 0000 0000 0000 0000, knocking 3 bits out of the exponent.
		//and yes, we could store uint64 in the lookup table, but that actually turns out to suck for cache.
		//and we're very cache sensitive where this is used, sadly. it's probably not worth the effort it took but... hey.

		//48 bits remaining. fuckin' majora's bitmask in here...
		return begin_compose | (uint64(NormalLUT[mbitsB]) << 24) | uint64(NormalLUT[mbitsA]);
	}

	//this produces a compressed float-flipped and z-ordered interleave of x,y,z (0bxyz[..]xyz)
	//by selectively annihilating important parts of the IEEE754 standard. each dim gets 21 bits.
	//extending the z curve into 3 has some Problems that make adjacency.... iffy. Also, please remember that
	//UE uses centimeters. This code works pretty well in that space... but may be donked in the Jolt Space.
	//later, we'll need to sit down and get serious about that.
	static uint64_t ComposeVoxelCode(float x, float y, float z)
	{
		return FloatToVoxelBits(x) | (FloatToVoxelBits(y) >> 1) | (FloatToVoxelBits(z) >> 2);
	}

	constexpr static uint64_t DwarvenSieveX = 0b0100100100100100100100100100100100100100100100100100100100100100;
	constexpr static uint64_t DwarvenSieveY = 0b0010010010010010010010010010010010010010010010010010010010010010;
	constexpr static uint64_t DwarvenSieveZ = 0b0001001001001001001001001001001001001001001001001001001001001001;
	//earthmover -> dwarven. this one's super cool but not obviously useful.

	static uint64_t unmorton3(uint64_t x)
	{
		x = x & 0x9249249249249249;
		x = (x | (x >> 2)) & 0x30c30c30c30c30c3;
		x = (x | (x >> 4)) & 0xf00f00f00f00f00f;
		x = (x | (x >> 8)) & 0x00ff0000ff0000ff;
		x = (x | (x >> 16)) & 0xffff00000000ffff;
		x = (x | (x >> 32)) & 0x00000000ffffffff;
		return x;
	}

	static uint32_t KDwarvenDistance(uint64_t VoxelA, uint64_t VoxelB)
	{
		auto X = ((VoxelA & DwarvenSieveX) - (VoxelB & DwarvenSieveX)) >> 2;
		auto Y = ((VoxelA & DwarvenSieveY) - (VoxelB & DwarvenSieveY)) >> 1;
		auto Z = (VoxelA & DwarvenSieveY) - (VoxelB & DwarvenSieveZ);

		//diagonal steps are worth less, but not nothing.
		auto TotalUniqueHammingWeightOfDistances =
			AtypicalDistances::CountBits((Y | Z) ^ X) + AtypicalDistances::CountBits((X | Z) ^ Y) + AtypicalDistances::CountBits((X | Y) ^ Z);
		return (2 * TotalUniqueHammingWeightOfDistances) + AtypicalDistances::CountBits(X) + AtypicalDistances::CountBits(Y) + AtypicalDistances::CountBits(Z);
	}

	static uint32_t MinerDistance(uint64_t VoxelA, uint64_t VoxelB)
	{
		uint64_t a = unmorton3(VoxelA);
		uint64_t b = unmorton3(VoxelA >> 1);
		uint64_t c = unmorton3(VoxelA >> 2); //don't think of them as xyz, because they _may not be_.
		a = a - unmorton3(VoxelB);
		b = b - unmorton3(VoxelB >> 1);
		c = c - unmorton3(VoxelB >> 2);
		return std::max(a, std::max(b, c)) + 2 * ((a + b + c) / 3); //manhattan lets you go to weird places.
	}


	//expand 10 bits for morton encoding. from jolt morton.
	static uint32 sExpand10Bits(uint32 v)
	{
		v &= AtypicalDistances::tenslice; // this forces the matter.
		v = (v * 0x00010001u) & 0xFF0000FFu;
		v = (v * 0x00000101u) & 0x0F00F00Fu;
		v = (v * 0x00000011u) & 0xC30C30C3u;
		v = (v * 0x00000005u) & 0x49249249u;
		return v;
	}


	/*
	 * 
	 * source: IV. C. Line-point Embedding: Affine Case, from Fast Probabilistic Collision Checking for Sampling-based Motion Planning,
	 * using locality sensitive hashing, by Jia Pan and Dinesh Manocha. You will want to be familiar with the original LSH paper by
	 * Andoni and Indyk if you're planning to read that. I asked Pan & Manocha for the source code, but had no luck.
	 *
	 * Significant liberties have been taken for the sake of actually rendering this in a comment without needing doxy or latex support.
	 * 
	 *  Consider the case of the arbitrary line that does not pass through origin. note, this is not a line segment.
	 *
	 *  For the sake of clarity, any line can in R-D space can be described as LINE = {A + S*V} where A is a point and V is a unit vector
	 *  in that space. S is the scalar used to project the line. Going forward, we will be primarily using 3d space for our input points,
	 *  BUT will be generating higher D vectors so do mind that. Note that ultimately, this hinges on a Linial-London-Robinovich embed
	 *
	 *  ---------------------
	 *  Preface: DO NOT SKIP.
	 *  ---------------------
	 *	1) We'll be using a constant, CM, where  max(x) ∈D = H, and CM > ||H||^2 + 1), where D is our set.
	 *	So length( max(x in SET))^2 +1. Remember this definition, because it helps to make sense of what's coming.
	 *  2) I is the identity matrix
	 *  3) they define a variant vectorization operation vec(·) such that...
	 *  the vectorization of an m×n matrix A is the mn x 1 column vector obtained by stacking the columns of the matrix
	 *  on top of one another: vec(A)= ┏a[1][1]
	 *										...
	 *									a[m][1]
	 *									a[1][2]
	 *										...
	 *									a[m][2],
	 *										...
	 *									a[1][n]
	 *										...
	 *									a[m][n]┛, which as promised produces an mn height column vector.
	 *  a[i][j] is denoting (i, j)-th element of original matrix A.

	 *	4) We'll also be using the matrix trace operation. I'll be using MTrace for that.
	 *	5) Euclidean distance of a point X to the LINE is dist^2 (x, l) = (x − a) · (x − a) − ((x − a) · v)^2
	 *	6) non-affine, AGAIN, non-affine embedding of the line L starts as a d+1 matrix MU:
	*		 Matrix	┏								┓	
	 *		  of	┃	I-(v transpose(v))		0	┃
	 *				┃			0				0	┃
	 *	            ┗ 								┛  AND I-(v transpose(v) is a SUB MATRIX, resulting in D+1 x D+1 matrix
	 *	and is THEN converted into a vector using their vec(·) operator as Vl(v) -> vec(MU)  
	 *	----------------
	 *	Embedding a line
	 *	----------------
	 *	For any line, we may write it in the form:
	 *  LINE = {A + S*V}
	 *	There exists a distance such that:
	 *	dist^2 (POINT, LINE)
	 *	= (POINT - a) · (POINT - a) - ((POINT-a) · v)^2
	 *	= MTrace(		transpose(x - a)	(I - (v)(transpose(v)))	(x - a)
	 *
	 *	preface:
	 *	Y = Matrix	┏ x ┓	x, 1, t are all vec components.
	 *		  of	┃ 1 ┃
	 *	            ┗ t ┛	where t is defined in terms of x! 
	 *	            AGAIN. T is actually t(x). Here, we use c from earlier! optimally, t(x) = sqrt(CM - ||x||^2 -1)
	 *	IM = ( I   -a   0)
	 *	B = transpose( IM )   (I - (v)(transpose(v)))   IM , note the repetition of the middle term from earlier.
	 *
	 *	composing B as intended remains the troublesome part.
	 *
	 *	given that,
	 *  dist^2 (POINT, LINE) = MTrace(transpose(Y)		B		Y)
	 *  AND dist^2 (POINT, LINE) = vectorization_of_matrix( Y   transpose(y)   ) dot vectorization_of_matrix(B)
	 *  this produces a distance metric such that
	 *  = Vp(x) · Vl(v,a), where Vp and Vl are (d+ 2)^2 embeddings.
	 *  Specifically, for us, they are 25d embeddings for the point and line respectively. Wild, I know.
	 *	They are linearly dependent on the squared euclidean distance between the embedding set (our set of embedded points)
	 *  and the query item, or in this case, our line. That relationship can be written as:
	 *  LENGTH( Vp(x) - Vl(v,a)^2 == c^2 + d - 2 + (dist^2 (0,l) + 1)^2 + 2(dist^2(x,l))
	 *	This was pretty inobvious to me, actually.
	 *
	 *	Finally, and I quote because "the matrices used within vec(·) are symmetric"
	 *	for a d x d (3x3) matrix A, we can define a d(d+1)/2 dim embedding vectorization as follows
	 *	AM = vec(·) of A, per their definition in preface 3. This gives the following columnar vector:
	 *
	 *	THIS IS WRONG SOMEHOW. do we really drop those components? 
	 	┏	a[1][1]/sqrt(2)	┓
		┃	a[1][2]			┃
		┃	a[1][3]			┃
		┃	a[2][2]/sqrt(2)	┃
		┃	a[2][3]			┃
		┗	a[3][3]/sqrt(2) ┛  for a six dim vector. I don't think we actually use a 3x3 though...
	 *	And this basically makes sense, but it was definitely inobvious to me. the linear dependence injection & a vect means
	 *	that looks like they reused the d notation, actually, so d should be d` = d+2.
	 *	The B term does end up symmetric, and it's d+2 x d+2 as expected. So it looks like the B term is our sym matrix A.
	 *	if so...
	 *	EDir = {{1 - x^2, -x y, -x z}, {-x y, 1 - y^2, -y z}, {-x z, -y z, 1 - z^2}}
	 *	EAffine = {{1, 0, 0, a, 0}, {0, 1, 0, b, 0}, {0, 0, 1, c, 0}}
	 *	B = transpose(EAffine) * EDir * EAffine
	 *	
	 *	so.... maybe you get something like...
	 *		(1 - x^2) /sqrt(2)
	 *		-x y
	 *		-x z
	 *		a(1 - x^2) - b x y - c x z
	 *		0
	 *		(1 - y^2) /sqrt(2)
	 *		-y z
	 *		-a x y + b (1 - y^2) - c y z
	 *		0
	 *		(1 - z^2)/sqrt(2)
	 *		-a x z - b y z + c (1 - z^2)
	 *		0
	 *		a (a (1 - x^2) - b x y - c x z) + b (-a x y + b (1 - y^2) - c y z) + c (-a x z - b y z + c (1 - z^2)) /sqrt(2)
	 *		0
	 *		0	
	 *	and actually, that does have most of the information I'd expect in the embedding.
	*	sweet jesus. I think this is _right_. at the very least, I think it will work.
	*
	*	I also recommend reading https://en.wikipedia.org/wiki/Moser%E2%80%93de_Bruijn_sequence
	*
	*	I've condensed the actual matrix CONSIDERABLY in a way that's not entirely safe, just to get some more shannon entropy.
	*	The presence of constants in the vectorization was extremely bad for hashing.
	*/
	struct FPLEmbed
	{
		
		uint64 voxeldata[3];
		static constexpr uint8 Size = 3;

		static FPLEmbed FromPoint(FVector3f A, float MaxExtent)
		{
			return FromPoint(A.X, A.Y, A.Z, MaxExtent);
		}


		static FPLEmbed FromPoint(float X, float Y, float Z, float MaxExtent)
		{
			double len = (X*X)+(Y*Y)+(Z*Z);
			float T = sqrt(MaxExtent - len - 1);
			float xx = (X * X);
			float xy = X * Y;
			float xz = X * Z;
			float tx = T * X;
			float yy = (Y * Y);

			float yz = (Y * Z);
			float ty = T * Y;

			float zz = (Z * Z);
			float tz = T * Z;

			float tm = T * T;
			FPLEmbed ret;



			ret.voxeldata[0] = ComposeVoxelCode(xy, yz, xz);
			ret.voxeldata[1] = ComposeVoxelCode(tx, ty, tz);
			ret.voxeldata[2] = ComposeVoxelCode(yy+tm, xx+tm, zz+tm);
			return ret;
		}

		static FPLEmbed FromLine(FVector3f Source, FVector3f Direction)
		{
			
			return FromLine(Source.X, Source.Y, Source.Z, Direction.X, Direction.Y, Direction.Z);
		}

		static FPLEmbed FromLine(float x, float y, float z, float a, float b, float c)
		{
			//the names get inaccurate here but I'm matching them to the PEmb
			//cause otherwise organizing the componentwise morton encode is hell.

			//tbh, I'm worried I may have done my math wrong.
			
			float xx = (1 - x * x) ;
			float xy = -1 * x * y;
			float xz = -1 * x * z;

			float xm = a*(1 - x * x) - b * x * y - c * x * z;
			float yy = (1 - y * y);

			float yz = -1 * y * z;
			float ym = -1*(a * x * y) + b*(1 - y * y) - c * y * z;

			float zz = (1 - z * z);
			float zm = -1*(a * x * z) - (b * y * z) + c*(1 - z * z);

			float sm = (
					  a*(a * (1 - x * x) - b * x * y - c * x * z)
					+ b*(-a * x * y + b*(1 - y * y) - c * y * z)
					+ c*(-a * x * z - b * y * z + c*(1 - z * z))
					);

			//morton coding actually ADDS relationship data, interestingly.
			//it took me a long time to understand this, but the gist is, if it didn't,
			//you wouldn't need to decompose the components to do most operations.
			//it loses information in exchange for adding that relationship.
			//in fact, this is what makes it a fractal space-filling curve, in a sense.
			FPLEmbed ret;
			ret.voxeldata[0] = ComposeVoxelCode(xy, yz, xz);
			ret.voxeldata[1] = ComposeVoxelCode(xm, ym, zm);
			ret.voxeldata[2] = ComposeVoxelCode(yy+sm, xx+sm, zz+sm);
			//looking over our vector components, it seems likely that we can drive this down to 12 functional
			//dimensions by using one or two additional tricks.
			return ret;
		}

		// ALWAYS BET ON LEBESGUE
		//
		// Generate an alternative line embedding to increase entropy by relating the a and v points directly.
		// this is done using a Z-Z interleave, or a Zer curve as I've been jokingly calling it, and a winding
		// rotation. A winding rotation here actually encodes a little more info than you'd expect.
		static inline FPLEmbed embedLineL1(
			float ax, float ay, float az,
			float vx, float vy, float vz)
		{
			FPLEmbed ret;
			auto a = FVector(ax, ay, az);
			auto v = FVector(vx, vy,vz);
			FRotator Point = v.Rotation();
			FRotator Dir = a.Rotation();
			// THIS PRESERVES the winding. It's equivalent to rotating to V from neutral THEN rotating to A.
			auto FixedWinding = (Point.Quaternion() * Dir.Quaternion()).Rotator(); 
			ret.voxeldata[0] = ComposeVoxelCode( FixedWinding.Pitch,  FixedWinding.Yaw,  FixedWinding.Roll);
			auto tempA = ComposeVoxelCode(ax, ay, az);
			auto tempV = ComposeVoxelCode(vx,vy,vz);
			auto highA = tempA >> 32;
			auto lowA = (tempA << 32) >> 32; //bonkbonk
			
			auto highV = tempV >> 32;
			auto lowV = (tempV << 32) >> 32; //bonkbonkBONK
			ret.voxeldata[1] = libmorton::morton2D_64_encode(highA, highV);
			ret.voxeldata[2] = libmorton::morton2D_64_encode(lowA, lowV);
			//looking over our vector components, it seems likely that we can drive this down to 12 functional
			//dimensions by using one or two additional tricks.
			return ret;
		}
	};



};
