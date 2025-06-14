// Copyright 2025 Oversized Sun Inc. All Rights Reserved.

#pragma once

#include <cstdint>
#include <chrono>

//centralizing the typedefs to avoid circularized header includes
//and further ease swapping over between 8 and 16 byte modes. IWYU!
class CABLING_API NarrowClock {
public:
	static uint32_t getSlicedMicrosecondNow()
	{
		using namespace std::chrono;
		return duration_cast<duration<uint32_t, std::micro>>(system_clock::now().time_since_epoch()).count();
	}
};