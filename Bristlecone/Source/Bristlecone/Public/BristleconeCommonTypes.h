// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "FBristleconePacket.h"
#include "FFastBitTracker.h"
#include "UnsignedNarrowTime.h"
#include "FControllerState.h"
#include "Containers/CircularQueue.h"
#include <cstdint>

//centralizing the typedefs to avoid circularized header includes
//and further ease swapping over between 8 and 16 byte modes. IWYU!
namespace TheCone {
	typedef uint64_t PacketElement;
	typedef FBristleconePacket<PacketElement, 3> Packet_tpl;
	typedef std::pair<uint32_t, long> CycleTimestamp;
	typedef TCircularQueue<Packet_tpl> PacketQ;
	typedef TCircularQueue<PacketElement> IncQ;
	typedef TCircularQueue<CycleTimestamp> TimestampQ;
	typedef TSharedPtr<PacketQ, ESPMode::ThreadSafe> RecvQueue; // it is the default, but let's be explicit.
	typedef TSharedPtr<TimestampQ, ESPMode::ThreadSafe> TimestampQueue;
	typedef TSharedPtr<TCircularQueue<PacketElement>, ESPMode::ThreadSafe> SendQueue; // note that the queues only support 1p1c mode.
	typedef FBristleconePacket<FControllerState, 3> FControllerStatePacket;
	constexpr uint32_t LongboySendHertz = 128;
	constexpr uint32_t CablingSampleHertz = 512;
	constexpr uint32_t BristleconeSendHertz = 90;

	static constexpr int CONTROLLER_STATE_PACKET_SIZE = sizeof(FControllerStatePacket);
	static constexpr int DEFAULT_PORT = 40000;
	static constexpr uint16 MAX_TARGET_COUNT = 1;
	static constexpr float SLEEP_TIME_BETWEEN_THREAD_TICKS = 0.008f;
	static constexpr uint8 CLONE_SIZE = 3;
	static constexpr uint8 MAX_MIXED_CONSECUTIVE_PACKETS_ALLOWED = 100;

	/*This class generalizes and defactors tracking the last K seen of a set. Right now, it's for cycles
	but nothing really stops you from using it for other stuff. Weirdly, it might be pretty easy to make
	this threadsafe using atomics, but it not currently threasafe.


	Originally based on this code:
	if ((cycle > HighestSeen))
	{
		//this isn't strictly needed but you shouldn't shift more than the width.
		cycle - HighestSeen > 64 ? SeenCycles = 0 : SeenCycles >>= (cycle - HighestSeen);
		HighestSeen = cycle;
	}
	//if it's off the bottom of the mask, we discard it.
	else if ((cycle - HighestSeen) < -64)
	{
		continue; //we don't accept inputs older than 64 cycles right now. TODO: make this configurable.
	}
	// if we've seen it, we discard it. Also, 1ull is awful.
	else if (SeenCycles & (1ull << (HighestSeen - cycle)))
	{
		continue;
	}
	else
	{
		//If we hadn't seen it, we set it now.
		SeenCycles |= (1ull << (HighestSeen - cycle));
	}
}
*/
	//today I learned that seer is just see-er. :|
	typedef FFastBitTracker CycleTracking;
}