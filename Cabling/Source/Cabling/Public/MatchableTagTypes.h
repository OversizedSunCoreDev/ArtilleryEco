// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include <optional>

#include "AtypicalDistances.h"
#include "MatchableTagTypes.generated.h"

USTRUCT(BlueprintType)
struct CABLING_API FMatchableInputlike
{
	GENERATED_BODY()
	virtual ~FMatchableInputlike() = default;
	virtual float GetStickLeftX() { return 0; }
	virtual int32_t GetStickLeftXAsACSN() { return 0; }
	virtual float GetStickLeftY() { return 0; }
	virtual int32_t GetStickLeftYAsACSN() { return 0; }
	virtual float GetStickRightX() { return 0; }
	virtual int32_t GetStickRightXAsACSN() { return 0; }
	virtual float GetStickRightY() { return 0; }
	virtual int32_t GetStickRightYAsACSN() { return 0; }
	virtual uint32 GetButtonsAndEventsFlat() { return 0; }
};

class CABLING_API MatchingTools
{
public:
	template <typename indirecting>
	class CABLING_API FTrueMinimumNoStandards
	{
	protected:
		~FTrueMinimumNoStandards() = default;

	public:
		virtual std::optional<indirecting> peek(uint64_t input) = 0;
	};

	//This is pretty good. Here it is, the most magical constant I've written.
	//we use the Cabling Integerized Sticks. Normally, we turn them into floats.
	//But squares of floats are a good way to blow your bit corruption limits for
	//FP rounding error. So let's use distance across a metric space, number of discretized
	//positions away from center on an axis. So we need a magnitude boundary to start
	//a stick flick detection from. This is that.
	//Tune as needed. we actually maintain a surprisingly finegrained degree of control here.
	
	constexpr static uint32_t ArtilleryMagicFlickBoundary = 662;
	constexpr static int32_t ArtilleryMagicMinimumFlickDistanceRequired = 742;
	constexpr static int32_t DefaultFlickTickWidth = 17;

	template <typename BindTo>
	bool static FlickDetect(int32_t curX, int32_t curY, uint64_t frameToRunBackFrom, uint64_t FinishIndex, BindTo Buffer)
	{
		using ATD = AtypicalDistances;
		int BoxRange = FMath::Max(abs( curX), abs( curY));
		if( BoxRange > ArtilleryMagicFlickBoundary)
		{
			for (uint64_t index = frameToRunBackFrom - 1; FinishIndex <= index; --index)
			{
				auto entry = Buffer->peek(index);
				auto x = entry->GetStickLeftXAsACSN();
				auto y = entry->GetStickLeftYAsACSN();
				if (ATD::OctagonalApproximateDistance(	x, y, curX, curY) >= ArtilleryMagicMinimumFlickDistanceRequired)
				{
					return true;
				}
			}
		}
		return false;
	}
};
