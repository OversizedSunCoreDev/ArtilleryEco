#pragma once
#include "MashFunctions.h"

//the maybe table exists at the intersection of probabilistic counting, bloomfilters, and braindead hashing.
//https://marc-b-reynolds.github.io/math/2019/08/10/Avalanche.html is a fairly similar process, but still not very.
struct FMaybeTable
{
public:
	constexpr static inline uint32_t width = 2041; 
	char Maybe[width] = {};

	std::pair<char, bool> Count(uint32_t dot)
	{
		uint32_t hashA = FMMM::FastHash32(dot+101);
		auto a = Maybe[hashA%width]++;
		uint32_t hashB = FMMM::FastHash32(dot+32);
		auto b = Maybe[hashB%width]++;
		return { std::min(a,b), a == b}; 
	}

	char CountNoParityCheck(uint32_t dot)
	{
		uint32_t hashA = FMMM::FastHash32(dot+101);
		auto a = Maybe[hashA%width]++;
		uint32_t hashB = FMMM::FastHash32(dot+32);
		auto b = Maybe[hashB%width]++;
		return std::min(a,b); 
	}
	
	//quite badly.
	char CountBadly(uint32_t dot)
	{
		auto hashA = FMMM::FastHash32(dot+32);
		return Maybe[hashA%width]++;
	}
};


