#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/StateRecorder.h>
#include <Jolt/Physics/StateRecorderImpl.h>
#include <unordered_map>
#include <memory>

#include "ArtilleryShell.h"
#include "SkeletonTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "StateContainer.generated.h"
// Forward declarations
class UBarrageDispatch;
class UArtilleryDispatch;


USTRUCT()
struct FArtilleryGenericData
{
    GENERATED_BODY()
};


// Stores the entire predicted state of one frame for rollback
USTRUCT()
struct FArtilleryDataBuffer
{
    GENERATED_BODY()
    
    //relies on the JPH::StateRecorderImpl operator= added in
    //RollbackBundle/JoltPatches/StateRecorderImpl.h.patch.
    FArtilleryDataBuffer() = default;
    FArtilleryDataBuffer(const FArtilleryDataBuffer& Other) noexcept
        : SequenceNumber(Other.SequenceNumber),
          bIsValid(Other.bIsValid),
          bIsVerified(Other.bIsVerified),
          TimeStamp(Other.TimeStamp),
          Inputs(Other.Inputs)
    {
        PhysicsData.Clear();
        /* TODO(#3, DEFERRED per JMK 2026-06-25): copy the physics snapshot via Jolt's public
		   StateRecorderImpl read/restore stream API, NOT operator= (modern Jolt deletes it). Stubbed
		   to compile while #1/#2 land; PhysicsData is NOT yet copied here. We test both ways later. */
    }

    FArtilleryDataBuffer& operator=(const FArtilleryDataBuffer& Other) noexcept
    {
        if (this != &Other)
        {
            SequenceNumber = Other.SequenceNumber;
            bIsValid = Other.bIsValid;
            Inputs = Other.Inputs;
            /* TODO (DEFERRED BY JMK 2026-06-25): 
             * copy the physics snapshot via Jolt's public
             * StateRecorderImpl read/restore stream API, NOT operator= (modern Jolt deletes it)
             * PhysicsData is NOT yet copied here. We test both ways later. 
             * 
             */
            TimeStamp = Other.TimeStamp;
            bIsVerified = Other.bIsVerified;
        }
        return *this;
    }

    UPROPERTY()
    uint32 SequenceNumber = 0;
    UPROPERTY()
    bool bIsValid = false;
    UPROPERTY()
    bool bIsVerified = false;
    
    ArtilleryTime TimeStamp = 0;
    TMap<PlayerKey, FArtilleryShell> Inputs;
    
    
    JPH::StateRecorderImpl PhysicsData;
    
    // Allows users to provide their own structs for serializing data in a more unreal-friendly manner
    UPROPERTY()
    TArray<TInstancedStruct<FArtilleryGenericData>> GenericData;
    
    // Raw bytes that can be used by any Artillery Object. Warning: mismatched offsets WILL explode upon read/write
    UPROPERTY()
    TArray<uint8> RawBytes;
};

class FArtilleryStateManager
{
public:
    FArtilleryStateManager(uint32 InBufferSize = 40)
    {
        BufferSize = InBufferSize;
        Ticks.Init(FArtilleryDataBuffer(), InBufferSize);
    };
    void Initialize(UWorld* World)
    {

    };
    void StoreTick(uint32 FrameNumber, const FArtilleryDataBuffer& Data, bool bIsVerified = false)
    {
        if (FrameNumber >= OldestSequence + BufferSize) {
            OldestSequence = FrameNumber - BufferSize + 1;
        }

        uint32 Index = FrameNumber % BufferSize;
        Ticks[Index] = Data;
        Ticks[Index].SequenceNumber = FrameNumber;
        Ticks[Index].bIsValid = true;
        Ticks[Index].bIsVerified = bIsVerified;
        SequenceRange = FMath::Max(SequenceRange, FrameNumber - OldestSequence + 1);

        if (bIsVerified)
        {
            VerifiedTick = Ticks[Index];
            LastVerifiedTick = FrameNumber;
        }
        //do not auto-promote the first frame stored; "verified" means the server said so or we've fallen out of recovery window,
        //and until that happens RollbackToVerified should no-op.
    }

    FArtilleryDataBuffer* GetTick(uint32 TickNumber)
    {
        if (VerifiedTick.bIsValid && VerifiedTick.SequenceNumber == TickNumber)
        {
            return &VerifiedTick;
        }

        if (TickNumber < OldestSequence || TickNumber >= OldestSequence + SequenceRange)
        {
            return nullptr;
        }
        uint32 Index = TickNumber % BufferSize;
        FArtilleryDataBuffer* Frame = &Ticks[Index];
        return (Frame->bIsValid && Frame->SequenceNumber == TickNumber) ? Frame : nullptr;
    }

    void StoreVerified(uint32 FrameNumber)
    {
        FArtilleryDataBuffer* VerifiedFramePtr = GetTick(FrameNumber);
        if (ensure(VerifiedFramePtr))
        {
            VerifiedFramePtr->bIsVerified = true;
            VerifiedFramePtr->bIsValid = true;
            VerifiedTick = *VerifiedFramePtr;
            LastVerifiedTick = FrameNumber;
        }
    }

    void Shutdown()
    {
        Ticks.Empty();
        VerifiedTick = FArtilleryDataBuffer();
        LastVerifiedTick = 0;
    }

    bool HasVerifiedFrame() const { return VerifiedTick.bIsValid; }
    uint32 GetLastVerifiedSequence() const { return LastVerifiedTick; }
    uint32 GetOldestSequence() const { return OldestSequence; }

private:
    TArray<FArtilleryDataBuffer> Ticks;
    FArtilleryDataBuffer VerifiedTick;
    uint32 BufferSize = 20;
    uint32 OldestSequence = 0;
    uint32 SequenceRange = 0;
    uint32 LastVerifiedTick = 0;
};
