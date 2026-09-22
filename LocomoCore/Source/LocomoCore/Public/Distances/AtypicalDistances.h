#pragma once
#include <bit>

#include "LCM_Config.h"
#include "CoreMinimal.h"
#include "Eigen/StdVector"
/****
 *
 *Portions of this file are derived from FLANN. The following is reproduced as a result.
The BSD License

Copyright (c) 2008-2011  Marius Muja. All rights reserved.
Copyright (c) 2008-2011  David G. Lowe. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
****/


//this is a small collection of distances that really can't live much of anywhere else,
//or functions that are used by all the other distances in some capacity or another.
//This in many ways is one of the most important and useful parts of locomo.
class AtypicalDistances
{
public:
	static constexpr uint32 tenslice = 0b00000000000000000000001111111111;

	// ================================================================================================
	// flip bits of a float for sorting. you likely wanna use LIMITS first. but maybe not.
	//  finds SIGN of fp number.
	//  if it's 1 (negative float), it flips all bits
	//  if it's 0 (positive float), it flips the sign only
	// ================================================================================================
	static inline uint32 FloatFlip(uint32 f)
	{
		uint32 mask = -int32(f >> 31) | 0x80000000;
		return f ^ mask;
	}

	//taxicab geometry angle of a vector to origin, do you see where this is going?
	auto DiamondAngle(float y, float x)
	{
		if (y >= 0)
			return (x >= 0 ? y/(x+y) : 1-x/(-x+y)); 
		else
			return (x < 0 ? 2-y/(-x-y) : 3+x/(x-y)); 
	}
	// ================================================================================================
	// flip a float back (invert FloatFlip)
	//  signed was flipped from above, so:
	//  if sign is 1 (negative), it flips the sign bit back
	//  if sign is 0 (positive), it flips all bits back
	// ================================================================================================
	static inline uint32 IFloatFlip(uint32 f)
	{
		uint32 mask = ((f >> 31) - 1) | 0x80000000;
		return f ^ mask;
	}

	template <typename T>
	struct Accumulator
	{
		typedef T Type;
	};

	template <>
	struct Accumulator<unsigned char>
	{
		typedef float Type;
	};

	template <>
	struct Accumulator<unsigned short>
	{
		typedef float Type;
	};

	template <>
	struct Accumulator<unsigned int>
	{
		typedef float Type;
	};

	template <>
	struct Accumulator<char>
	{
		typedef float Type;
	};

	template <>
	struct Accumulator<short>
	{
		typedef float Type;
	};

	template <>
	struct Accumulator<int>
	{
		typedef float Type;
	};

	//https://www.flipcode.com/archives/Fast_Approximate_Distance_Functions.shtml
	//how incredibly strange. it creates a sort of faceted
	//metric space, which is really really useful for thinking about joysticks and movement
	//and it's very fast to compute. It doesn't actually require any FP ops, either.
	//It's less accurate than I'd like, and while I broadly understand the math,
	//I'm not comfortable enough with it to sit down and knock the bugs out.
	uint32_t static OctagonalApproximateDistance(int x1, int y1, int x2, int y2)
	{
		return OctagonalApproximateDistance(x2 - x1, y2 - y1);
	}

	uint32_t static OctagonalApproximateDistance(int dx, int dy)
	{
		uint32_t min, max, approx, x, y;
		x = abs(dx);
		y = abs(dy);
		if (x < y)
		{
			min = x;
			max = y;
		}
		else
		{
			min = y;
			max = x;
		}

		approx = (max * 1007) + (min * 441);
		if (max < (min << 4))
		{
			approx -= (max * 40);
		}
		// add 512 for proper rounding

		return ((approx + 512) >> 10);
	}



	//shims
	inline static uint32_t CountBits(uint64_t v) { return (uint32_t)std::popcount(v); }
	inline static uint64_t CountBits64(uint64_t v) { return (uint64_t)std::popcount(v); }

	//this is a set of pretty normal distance functions
	//from flann. most of these are already available, but that said
	//// it's worth having them here as examples. --JMK

	/**
	 * Squared Euclidean distance functor.
	 *
	 * This is the simpler, unrolled version. This is preferable for
	 * very low dimensionality data (eg 3D points)
	 */
	template <class T>
	struct L2_Simple
	{
		typedef bool is_kdtree_distance;

		typedef T ElementType;
		typedef typename Accumulator<T>::Type ResultType;

		template <typename Iterator1, typename Iterator2>
		ResultType operator()(Iterator1 a, Iterator2 b, size_t size, ResultType /*worst_dist*/  = -1) const
		{
			ResultType result = ResultType();
			ResultType diff;
			for (size_t i = 0; i < size; ++i)
			{
				diff = *a++ - *b++;
				result += diff * diff;
			}
			return result;
		}

		template <typename U, typename V>
		inline ResultType accum_dist(const U& a, const V& b, int) const
		{
			return (a - b) * (a - b);
		}
	};

	template <class T>
	struct L2_3D
	{
		typedef bool is_kdtree_distance;

		typedef T ElementType;
		typedef typename Accumulator<T>::Type ResultType;

		template <typename Iterator1, typename Iterator2>
		ResultType operator()(Iterator1 a, Iterator2 b, size_t size, ResultType /*worst_dist*/  = -1) const
		{
			ResultType result = ResultType();
			ResultType diff;
			diff = *a++ - *b++;
			result += diff * diff;
			diff = *a++ - *b++;
			result += diff * diff;
			diff = *a++ - *b++;
			result += diff * diff;
			return result;
		}

		template <typename U, typename V>
		inline ResultType accum_dist(const U& a, const V& b, int) const
		{
			return (a - b) * (a - b);
		}
	};

	/**
	 * Squared Euclidean distance functor, optimized version
	 */
	template <class T>
	struct L2
	{
		typedef bool is_kdtree_distance;

		typedef T ElementType;
		typedef typename Accumulator<T>::Type ResultType;

		/**
		 *  Compute the squared Euclidean distance between two vectors.
		 *
		 *	This is highly optimised, with loop unrolling, as it is one
		 *	of the most expensive inner loops.
		 *
		 *	The computation of squared root at the end is omitted for
		 *	efficiency.
		 */
		template <typename Iterator1, typename Iterator2>
		ResultType operator()(Iterator1 a, Iterator2 b, size_t size, ResultType worst_dist = -1) const
		{
			ResultType result = ResultType();
			ResultType diff0, diff1, diff2, diff3;
			Iterator1 last = a + size;
			Iterator1 lastgroup = last - 3;

			/* Process 4 items with each loop for efficiency. */
			while (a < lastgroup)
			{
				diff0 = (ResultType)(a[0] - b[0]);
				diff1 = (ResultType)(a[1] - b[1]);
				diff2 = (ResultType)(a[2] - b[2]);
				diff3 = (ResultType)(a[3] - b[3]);
				result += diff0 * diff0 + diff1 * diff1 + diff2 * diff2 + diff3 * diff3;
				a += 4;
				b += 4;

				if ((worst_dist > 0) && (result > worst_dist))
				{
					return result;
				}
			}
			/* Process last 0-3 pixels.  Not needed for standard vector lengths. */
			while (a < last)
			{
				diff0 = (ResultType)(*a++ - *b++);
				result += diff0 * diff0;
			}
			return result;
		}

		/**
		 *	Partial euclidean distance, using just one dimension. This is used by the
		 *	kd-tree when computing partial distances while traversing the tree.
		 *
		 *	Squared root is omitted for efficiency.
		 */
		template <typename U, typename V>
		inline ResultType accum_dist(const U& a, const V& b, int) const
		{
			return (a - b) * (a - b);
		}
	};


	/*
	 * Manhattan distance functor, optimized version
	 */
	template <class T>
	struct L1
	{
		typedef bool is_kdtree_distance;

		typedef T ElementType;
		typedef typename Accumulator<T>::Type ResultType;

		/**
		 *  Compute the Manhattan (L_1) distance between two vectors.
		 *
		 *	This is highly optimised, with loop unrolling, as it is one
		 *	of the most expensive inner loops.
		 */
		template <typename Iterator1, typename Iterator2>
		ResultType operator()(Iterator1 a, Iterator2 b, size_t size, ResultType worst_dist = -1) const
		{
			ResultType result = ResultType();
			ResultType diff0, diff1, diff2, diff3;
			Iterator1 last = a + size;
			Iterator1 lastgroup = last - 3;

			/* Process 4 items with each loop for efficiency. */
			while (a < lastgroup)
			{
				diff0 = (ResultType)std::abs(a[0] - b[0]);
				diff1 = (ResultType)std::abs(a[1] - b[1]);
				diff2 = (ResultType)std::abs(a[2] - b[2]);
				diff3 = (ResultType)std::abs(a[3] - b[3]);
				result += diff0 + diff1 + diff2 + diff3;
				a += 4;
				b += 4;

				if ((worst_dist > 0) && (result > worst_dist))
				{
					return result;
				}
			}
			/* Process last 0-3 pixels.  Not needed for standard vector lengths. */
			while (a < last)
			{
				diff0 = (ResultType)std::abs(*a++ - *b++);
				result += diff0;
			}
			return result;
		}

		/**
		 * Partial distance, used by the kd-tree.
		 */
		template <typename U, typename V>
		inline ResultType accum_dist(const U& a, const V& b, int) const
		{
			return std::abs(a - b);
		}
	};


	template <class T>
	struct MinkowskiDistance
	{
		typedef bool is_kdtree_distance;

		typedef T ElementType;
		typedef typename Accumulator<T>::Type ResultType;

		int order;

		MinkowskiDistance(int order_) : order(order_)
		{
		}

		/**
		 *  Compute the Minkowsky (L_p) distance between two vectors.
		 *
		 *	This is highly optimised, with loop unrolling, as it is one
		 *	of the most expensive inner loops.
		 *
		 *	The computation of squared root at the end is omitted for
		 *	efficiency.
		 */
		template <typename Iterator1, typename Iterator2>
		ResultType operator()(Iterator1 a, Iterator2 b, size_t size, ResultType worst_dist = -1) const
		{
			ResultType result = ResultType();
			ResultType diff0, diff1, diff2, diff3;
			Iterator1 last = a + size;
			Iterator1 lastgroup = last - 3;

			/* Process 4 items with each loop for efficiency. */
			while (a < lastgroup)
			{
				diff0 = (ResultType)std::abs(a[0] - b[0]);
				diff1 = (ResultType)std::abs(a[1] - b[1]);
				diff2 = (ResultType)std::abs(a[2] - b[2]);
				diff3 = (ResultType)std::abs(a[3] - b[3]);
				result += pow(diff0, order) + pow(diff1, order) + pow(diff2, order) + pow(diff3, order);
				a += 4;
				b += 4;

				if ((worst_dist > 0) && (result > worst_dist))
				{
					return result;
				}
			}
			/* Process last 0-3 pixels.  Not needed for standard vector lengths. */
			while (a < last)
			{
				diff0 = (ResultType)std::abs(*a++ - *b++);
				result += pow(diff0, order);
			}
			return result;
		}

		/**
		 * Partial distance, used by the kd-tree.
		 */
		template <typename U, typename V>
		inline ResultType accum_dist(const U& a, const V& b, int) const
		{
			return pow(static_cast<ResultType>(std::abs(a - b)), order);
		}
	};


	template <class T>
	struct MaxDistance
	{
		typedef bool is_vector_space_distance;

		typedef T ElementType;
		typedef typename Accumulator<T>::Type ResultType;

		/**
		 *  Compute the max distance (L_infinity) between two vectors.
		 *
		 *  This distance is not a valid kdtree distance, it's not dimensionwise additive.
		 */
		template <typename Iterator1, typename Iterator2>
		ResultType operator()(Iterator1 a, Iterator2 b, size_t size, ResultType worst_dist = -1) const
		{
			ResultType result = ResultType();
			ResultType diff0, diff1, diff2, diff3;
			Iterator1 last = a + size;
			Iterator1 lastgroup = last - 3;

			/* Process 4 items with each loop for efficiency. */
			while (a < lastgroup)
			{
				diff0 = std::abs(a[0] - b[0]);
				diff1 = std::abs(a[1] - b[1]);
				diff2 = std::abs(a[2] - b[2]);
				diff3 = std::abs(a[3] - b[3]);
				if (diff0 > result) { result = diff0; }
				if (diff1 > result) { result = diff1; }
				if (diff2 > result) { result = diff2; }
				if (diff3 > result) { result = diff3; }
				a += 4;
				b += 4;

				if ((worst_dist > 0) && (result > worst_dist))
				{
					return result;
				}
			}
			/* Process last 0-3 pixels.  Not needed for standard vector lengths. */
			while (a < last)
			{
				diff0 = std::abs(*a++ - *b++);
				result = (diff0 > result) ? diff0 : result;
			}
			return result;
		}

		/* This distance functor is not dimension-wise additive, which
		 * makes it an invalid kd-tree distance, not implementing the accum_dist method */
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
	 * Hamming distance - counts the bit differences between two strings - useful for the Brief descriptor
	 * bit count of A exclusive XOR'ed with B
	 */

	static uint32 hamming_32(uint32 a, uint32 b)
	{
		return CountBits(a ^ b);
	}

	static uint64 hamming_64(uint64 a, uint64 b)
	{
		return CountBits(a ^ b);
	}

	template <class T>
	struct HistIntersectionDistance
	{
		typedef bool is_kdtree_distance;

		typedef T ElementType;
		typedef typename Accumulator<T>::Type ResultType;

		/**
		 *  Compute the histogram intersection distance
		 */
		template <typename Iterator1, typename Iterator2>
		ResultType operator()(Iterator1 a, Iterator2 b, size_t size, ResultType worst_dist = -1) const
		{
			ResultType result = ResultType();
			ResultType min0, min1, min2, min3;
			Iterator1 last = a + size;
			Iterator1 lastgroup = last - 3;

			/* Process 4 items with each loop for efficiency. */
			while (a < lastgroup)
			{
				min0 = (ResultType)(a[0] < b[0] ? a[0] : b[0]);
				min1 = (ResultType)(a[1] < b[1] ? a[1] : b[1]);
				min2 = (ResultType)(a[2] < b[2] ? a[2] : b[2]);
				min3 = (ResultType)(a[3] < b[3] ? a[3] : b[3]);
				result += min0 + min1 + min2 + min3;
				a += 4;
				b += 4;
				if ((worst_dist > 0) && (result > worst_dist))
				{
					return result;
				}
			}
			/* Process last 0-3 pixels.  Not needed for standard vector lengths. */
			while (a < last)
			{
				min0 = (ResultType)(*a < *b ? *a : *b);
				result += min0;
				++a;
				++b;
			}
			return result;
		}

		/**
		 * Partial distance, used by the kd-tree.
		 */
		template <typename U, typename V>
		inline ResultType accum_dist(const U& a, const V& b, int) const
		{
			return a < b ? a : b;
		}
	};


	template <class T>
	struct HellingerDistance
	{
		typedef bool is_kdtree_distance;

		typedef T ElementType;
		typedef typename Accumulator<T>::Type ResultType;

		/**
		 *  Compute the Hellinger distance
		 */
		template <typename Iterator1, typename Iterator2>
		ResultType operator()(Iterator1 a, Iterator2 b, size_t size, ResultType /*worst_dist*/  = -1) const
		{
			ResultType result = ResultType();
			ResultType diff0, diff1, diff2, diff3;
			Iterator1 last = a + size;
			Iterator1 lastgroup = last - 3;

			/* Process 4 items with each loop for efficiency. */
			while (a < lastgroup)
			{
				diff0 = sqrt(static_cast<ResultType>(a[0])) - sqrt(static_cast<ResultType>(b[0]));
				diff1 = sqrt(static_cast<ResultType>(a[1])) - sqrt(static_cast<ResultType>(b[1]));
				diff2 = sqrt(static_cast<ResultType>(a[2])) - sqrt(static_cast<ResultType>(b[2]));
				diff3 = sqrt(static_cast<ResultType>(a[3])) - sqrt(static_cast<ResultType>(b[3]));
				result += diff0 * diff0 + diff1 * diff1 + diff2 * diff2 + diff3 * diff3;
				a += 4;
				b += 4;
			}
			while (a < last)
			{
				diff0 = sqrt(static_cast<ResultType>(*a++)) - sqrt(static_cast<ResultType>(*b++));
				result += diff0 * diff0;
			}
			return result;
		}

		/**
		 * Partial distance, used by the kd-tree.
		 */
		template <typename U, typename V>
		inline ResultType accum_dist(const U& a, const V& b, int) const
		{
			ResultType dist = sqrt(static_cast<ResultType>(a)) - sqrt(static_cast<ResultType>(b));
			return dist * dist;
		}
	};


	template <class T>
	struct ChiSquareDistance
	{
		typedef bool is_kdtree_distance;

		typedef T ElementType;
		typedef typename Accumulator<T>::Type ResultType;

		/**
		 *  Compute the chi-square distance
		 */
		template <typename Iterator1, typename Iterator2>
		ResultType operator()(Iterator1 a, Iterator2 b, size_t size, ResultType worst_dist = -1) const
		{
			ResultType result = ResultType();
			ResultType sum, diff;
			Iterator1 last = a + size;

			while (a < last)
			{
				sum = (ResultType)(*a + *b);
				if (sum > 0)
				{
					diff = (ResultType)(*a - *b);
					result += diff * diff / sum;
				}
				++a;
				++b;

				if ((worst_dist > 0) && (result > worst_dist))
				{
					return result;
				}
			}
			return result;
		}

		/**
		 * Partial distance, used by the kd-tree.
		 */
		template <typename U, typename V>
		inline ResultType accum_dist(const U& a, const V& b, int) const
		{
			ResultType result = ResultType();
			ResultType sum, diff;

			sum = (ResultType)(a + b);
			if (sum > 0)
			{
				diff = (ResultType)(a - b);
				result = diff * diff / sum;
			}
			return result;
		}
	};


	template <class T>
	struct KL_Divergence
	{
		typedef bool is_kdtree_distance;

		typedef T ElementType;
		typedef typename Accumulator<T>::Type ResultType;

		/**
		 *  Compute the Kullback–Leibler divergence
		 */
		template <typename Iterator1, typename Iterator2>
		ResultType operator()(Iterator1 a, Iterator2 b, size_t size, ResultType worst_dist = -1) const
		{
			ResultType result = ResultType();
			Iterator1 last = a + size;

			while (a < last)
			{
				if (*a != 0 && *b != 0)
				{
					ResultType ratio = (ResultType)(*a / *b);
					if (ratio > 0)
					{
						result += *a * log(ratio);
					}
				}
				++a;
				++b;

				if ((worst_dist > 0) && (result > worst_dist))
				{
					return result;
				}
			}
			return result;
		}

		/**
		 * Partial distance, used by the kd-tree.
		 */
		template <typename U, typename V>
		inline ResultType accum_dist(const U& a, const V& b, int) const
		{
			ResultType result = ResultType();
			if (a != 0 && b != 0)
			{
				ResultType ratio = (ResultType)(a / b);
				if (ratio > 0)
				{
					result = a * log(ratio);
				}
			}
			return result;
		}
	};
};
