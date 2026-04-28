#pragma once

#include <chrono>
#include "UnsignedNarrowTime.h"

template<
	typename CLONE_TYPE,
	unsigned int CLONE_SIZE>
class FBristleconePacket;

/**
 * Acts as a wrapper to manipulate the contents of a packet for networking
 * 
 * @tparam CLONE_TYPE 
 * @tparam CLONE_SIZE 
 */
template<
	typename CLONE_TYPE,
	unsigned int CLONE_SIZE>
class FBristleconePacketContainer {
public:
	FBristleconePacketContainer(): clone_state_ring_index(0) {
	}

	void InsertNewDatagram(const CLONE_TYPE* new_datagram) {
		// Update array with new data
		CLONE_TYPE* newest_element = packet.GetPointerToElement(clone_state_ring_index);
		memcpy(newest_element, new_datagram, sizeof(CLONE_TYPE));
		packet.UpdateTransferTime();

		// Update index
		clone_state_ring_index = (clone_state_ring_index + 1) % CLONE_SIZE;
	}

	FBristleconePacket<CLONE_TYPE, CLONE_SIZE>* GetPacket() {
		return &packet;
	}

	long GetSendTimeStamp() const {
		return packet.GetTransferTime();
	}

private:
	uint32 clone_state_ring_index;
	FBristleconePacket<CLONE_TYPE, CLONE_SIZE> packet;
};

/**
 * A Bristlecone packet, hereafter called a clone, contains at least the most recent datagram that we want to send
 * plus a history containing CLONE_SIZE - 1 datagrams
 * 
 * @tparam CLONE_TYPE Type used in the datagram.
 * @tparam CLONE_SIZE Count representing the number of datagrams to send with a single clone
 */
template<
	typename CLONE_TYPE,
	unsigned int CLONE_SIZE>
class FBristleconePacket {
public:
	FBristleconePacket() {
		Clear();
	}

	void Clear() {
		memset(clone_array, 0, sizeof(CLONE_TYPE) * CLONE_SIZE);
	}

	long GetTransferTime() const {
		return transfer_time;
	}
	
	void UpdateTransferTime() {
		uint32 new_time = NarrowClock::getSlicedMicrosecondNow();
		// verify it fits in the 16 bit range, if not, reset to 0 and start again. This means that if we have a packet that is delayed by more than 65ms, it will appear to have been sent just now, but that's still better than it appearing to have been sent in the far past. Or is it?
		if (new_time > TNumericLimits<uint16>::Max()) {
			transfer_time = 0;
		} else {
			transfer_time = static_cast<uint16>(new_time);
		}
	}

	long GetCycleMeta() const {
		return cycle_metadata;
	}

	void UpdateCycleOrMeta(uint16 update) {
		cycle_metadata = update;
	}

	void UpdateTransferTime(uint16 forceTimeStamp) {
		transfer_time = forceTimeStamp;
	}

	CLONE_TYPE* GetPointerToElement(uint32 element_index) {
		return &clone_array[element_index];
	}

	FString ToString() const {
		FString output;// = FString::Printf(TEXT("Transfer time = %s, array = "), *transfer_time.ToString());
		output += FString::Printf(TEXT("Transfer time = %lld"), transfer_time);
		output += ", array = "; 
		for (uint32 array_index = 0; array_index < CLONE_SIZE; array_index++) {
			output += clone_array[array_index].ToString();
			output += " ";
		}
		return output;
	}

private:
	// Packet headers
	//thanks to packing, there's not a ton of advantage to using a single long here
	//we could set the pack pragma and buy back the space, but ops on pack 1 objects
	//are often slower than they're worth. we're still under the 64 byte threshold
	//at 8*4 (32) + UDP\IP headers (28) with the 8 byte clone types.
	//When we have a 16 byte use case, I'll come back and tidy this up
	//by making those headers an optional type component
	//but for now, the extra debug info is really really useful.
	uint16 cycle_metadata;
	uint16 transfer_time;
	// Data clone
	CLONE_TYPE clone_array[CLONE_SIZE];
};
