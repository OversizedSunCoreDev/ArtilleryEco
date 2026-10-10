#pragma once

#include <algorithm>
#include <intrin.h>
#include "UObject/ObjectMacros.h"

//staleness minimizing cohered reader
struct FArrayGrouping
{
	
    short IdTracker = -1;
    short GenerateMyID()
    {
        return _InterlockedIncrement16(&IdTracker);
    }
};

// Staleness-minimizing cohered reader.
// Contract:
// - Fixed writer-owned heads; one writer per ID.
// - Writer publishes one value and one packed band/check word.
// - Reader accepts values from the leading observed band or the immediately prior band.
// - The top 16 bits of TSC carry a validator copied from the published double bits.
// - No C++ synchronization guarantee is claimed here; this is a low-level, platform-specific publication surface.
struct FTimedArray
{
    static constexpr uint32_t MaxWriters = 6;
    // 128 ticks. At 5 GHz, this is about 25.6 ns per band.
    static constexpr uint32_t BandShift = 7;

    // Packed TSC layout:
    //   bits 63..48: validator copied from double bits 52..37
    //   bits 47..0 : band
    static constexpr uint64_t ExtractTop16  = 0xffff000000000000ull;
    static constexpr uint64_t MaskTop16     = 0x0000ffffffffffffull;
    static constexpr uint64_t CheckSource16 = 0x001fffe000000000ull;
    static constexpr uint32_t CheckShift16  = 11;

    struct alignas(32) Heads
    {// Preserve 32 B/head
        double Value = 0.0;
        uint64 TSC = 0;
        uint64 Pad0 = 0;
        uint64 Pad1 = 0;
    };

    alignas(64) Heads BandedHeads[MaxWriters];
    static_assert(sizeof(Heads) == 32, "FTimeBandedArray::Heads must stay 32 bytes");
    static_assert(alignof(Heads) == 32, "FTimeBandedArray::Heads must stay 32-byte aligned");

    FORCEINLINE void Write(const double& In, const uint8& MyID)
    {
        const uint64 Band = __rdtsc() >> BandShift;
        Heads& Head = BandedHeads[MyID];
        Head.Value = In;
        Head.TSC = (Band & MaskTop16) | ((BitCast<uint64_t>(In) & CheckSource16) << CheckShift16);
    }

    FORCEINLINE double Candidate(const Heads& Head, uint64& Newest) const
    {
        const uint64 Packed = Head.TSC;
        const uint64 Band = Packed & MaskTop16;
        const double Value = Head.Value;
        const bool bValid =
            (((BitCast<uint64_t>(Value) & CheckSource16) << CheckShift16)
             == (Packed & ExtractTop16));

        Newest = std::max(Newest, Band);

        if (Band + 1 < Newest || !bValid)
        {
            return DOUBLE_BIG_NUMBER;
        }

        return Value;
    }

    // this is a min-tournament, similar to a sorting network.
    // I couldn't find a way to early out in case of just one candidate that was actually faster.
    FORCEINLINE double Read() const
    {
        uint64 Newest[6] = {}; //this MAY be able to move to the data structure level. MAY MAY MAY
        double V0 = Candidate(BandedHeads[0], Newest[0]);
        double V1 = Candidate(BandedHeads[1], Newest[1]);
        double V2 = Candidate(BandedHeads[2], Newest[2]);
        double V3 = Candidate(BandedHeads[3], Newest[3]);
        double V4 = Candidate(BandedHeads[4], Newest[4]);
        double V5 = Candidate(BandedHeads[5], Newest[5]);

        if (V1 < V0 && Newest[1] < Newest[0]) V0 = V1;
        if (V3 < V2 && Newest[3] < Newest[2]) V2 = V3;
        if (V5 < V4 && Newest[5] < Newest[4]) V4 = V5;

        if (V2 < V0 && Newest[2] < Newest[0]) V0 = V2;
        if (V4 < V0 && Newest[4] < Newest[0]) V0 = V4;

        return V0;
    }
};
