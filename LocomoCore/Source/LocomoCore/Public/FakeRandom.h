#pragma once

#include <array>

#include "CoreMinimal.h"
#include "MashFunctions.h"
#include "FakeRandom.generated.h"

//FFakeRandom supplies some intentionally low entropy and deterministic nudges and dispersions.
//This allows you to produce "learnable" spread cones or kick increments easily.
//In general, these functions take a "now" which is the seed and a "cycle" which is the step.
//Now is hashed, and cycle is added to the hash, then this is moduloed onto one of the spread tables.
//if speed becomes an issue here, the typehash can be replaced with lookup table and a modulo.
//if you do this, make sure your lookup table is of size K where K is 1.8*Hertz > K > hertz+sqrt(Hertz)
//and hertz is the tickrate of whatever you're using this with. this will produce a cycle longer than a second
//that doesn't repeat every second. It won't actually be less regular, but most people won't be able to actually
//tell that, and exploiting it will be quite difficult. Don't use this for loot. Do something actually smart there.
//note that we use an internal hash function, as typehash actually has undesirable behavior.
UCLASS()
class LOCOMOCORE_API UFakeRandom : public UObject
{
	GENERATED_BODY()
public:
	template<typename T, std::size_t LL, std::size_t RL>
	constexpr static std::array<T, LL+RL> t_join(std::array<T, LL> rhs, std::array<T, RL> lhs)
	{
		std::array<T, LL+RL> ar;

		auto current = std::copy(rhs.begin(), rhs.end(), ar.begin());
		std::copy(lhs.begin(), lhs.end(), current);

		return ar;
	}
	template<std::size_t LL, float mult>
		constexpr static std::array<float, LL> t_mult(std::array<float, LL> rhs)
	{
		auto multiply = [](float a) -> float { return a * mult; };
		std::array<float, LL> ar;
		std::for_each(
			rhs.begin(), rhs.end(),
			multiply
			);
		std::copy(rhs.begin(), rhs.end(), ar.begin());
		return ar;
	}

	template<typename T, std::size_t LL>
	constexpr static std::array<float, LL> t_offset(std::array<T, LL> rhs, const float add)
	{
		std::array<float, LL> ar = std::for_each(rhs.begin(), rhs.end(), [=](T a) -> float { return a+add; });
		return ar;
	}

	//to get a line, you can always just use one axis.
	enum Scattering
	{
		Ring,
		Cone
	};
	constexpr static uint8 NudgeTable = 48;
	constexpr static std::array<char, NudgeTable>  NudgeBy =
		{
		0, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 0, 0,
		0, 0, 1, 0,
		0,-1, 0, 0,
		0, 0, 0,-1,
		1, 1, 1, 1,
		0, 1, 1, 1,
		1, 1, 1, 1,
	   -1,-1,-1,-1,
	   -1,-1,-1,-1,
	   -1,-1, 0,-1
	   };
	constexpr static std::array<float, 12> x =
	{
		15, 12,  7,  0,
		-7,-12,-15,-12,
		-7,  0,  7, 12,
	};
	constexpr static std::array<float, 12> y =
	{
		0,   7,  12, 15,
	   12,   7,   0, -7,
	  -12, -15, -12, -7
  };

	constexpr static std::array<float, 12> z =
	{
		0,   7,  12, 15,
	   12,   7,   0, -7,
	  -12, -15, -12, -7
	 };

	//3d cube, used for laying down surrounding fire. centroid is 1/17 weighted, rather than 1/8.
	constexpr static uint32_t CubeBoxingLen = 17;
	constexpr static std::array<float, 8> CubeX = {1,-1,1,1,-1,-1,1,-1};
	constexpr static std::array<float, 8> CubeY = {1,-1,-1,1,-1,1,-1,1};
	constexpr static std::array<float, 8> CubeZ = {1,-1,-1,-1,1,1,1,-1};
	constexpr static std::array<float, 1> Zero = {0};
	constexpr static auto BoxingX =  t_join( t_join(CubeX, CubeX), Zero);
	constexpr static auto BoxingY =  t_join( t_join(CubeY, CubeY), Zero);
	constexpr static auto BoxingZ =  t_join( t_join(CubeZ, CubeZ), Zero);
	
	constexpr static uint32_t RingHRL = 12;
	constexpr static auto XInnerRing = t_mult<RingHRL, .7f>(x);
	constexpr static auto XInmostRing = t_mult<RingHRL, .3>(x);
	constexpr static auto YInnerRing = t_mult<RingHRL, .7>(y);
	constexpr static auto YInmostRing = t_mult<RingHRL,.3>(y);
	//-----
	constexpr static auto RingX = x;
	constexpr static auto RingY = y;
	//-----
	constexpr static uint32_t ConeHRL = 36;
	constexpr static auto ConeX = t_join( t_join(XInmostRing, XInnerRing), x);
	constexpr static auto ConeY = t_join( t_join(YInmostRing, YInnerRing), y);;
	//-----
	public:
	static FVector2D GetDispersion(uint32_t Now, Scattering Type, uint8 Cycle)
	{
		const auto len = Type == Ring ? RingHRL : ConeHRL;
		const auto initial = FMMM::FastHash32(Now);
		const auto finalXY = (initial + Cycle) % len;
		return Type == Ring ? FVector2D(RingX[finalXY], RingY[finalXY]) : FVector2D(ConeX[finalXY], ConeY[finalXY]);
	}

	//do not use the pointer or the skeleton key for the me value.
	static FVector GetBoxingDispersion(uint32_t Now, uint8 Cycle, uint16_t me)
	{
		//this means that as long as each caller is uniquely identified,
		//it will be a little like having a seed per caller while still basically just picking a point in the cube
		//and then walking the distribution in cycles. This produces the distinctive "fixed cluster" pattern of bungie guns.
		//adding and removing a little bit of mush here and there completes it, but this is just the basic round-robin fixed pseudo-pseudo random.
		const auto initial = FMMM::FastHash32(Now+me);	
		const auto finalXYZ = (initial + 7*Cycle) % CubeBoxingLen; //the seven hides the fact that we just step along the cube.
		return {BoxingX[finalXYZ], BoxingY[finalXYZ], BoxingZ[finalXYZ]};
	}

	static FVector2D GetNudge(uint32_t Now, uint8 Cycle)
	{
		const auto initialX = FMMM::FastHash32(Now);
		const auto initialY = FMMM::FastHash32(Now+1011);
		const auto finalX = (initialX + Cycle) % NudgeTable;
		const auto finalY = (initialY + Cycle) % NudgeTable;
		return FVector2D(NudgeBy[finalX], NudgeBy[finalY]);
	}

	static float SelectSlosh(uint32_t Now, uint8 Cycle)
	{
		const auto initialX = FMMM::FastHash32(Now+Cycle);
		return CoinFlip(Now,Cycle) * .0125 * (initialX % 3);
	}

	static int8 CoinFlip(uint32_t Now, uint8 Cycle)
	{
		const auto initialX = FMMM::FastHash32(Now+Cycle);
		return 1 - (2 * (initialX % 2));
	}

	static FVector2D GetSimpleSprayNudge(uint32_t Now, uint8 Cycle)
	{
		float wiggle = 1 + SelectSlosh(Now, Cycle);
		auto push = GetDispersion(Now, Cone, Cycle);
		return push*wiggle;
	}
};
