#pragma once
THIRD_PARTY_INCLUDES_START
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING
#include <windows.h>
#include <thread>
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END

#include "FJThread.generated.h"
//Hi! You might be wondering why this exists. Well, hopefully, you'll never see this text and we'll never use this.
//Ideally, you're just skimming through the ancient git history when... you see it!

//this is a paperthin wrapper around the c++20 jthread, which autojoins during destruction
USTRUCT()
struct FJThread{
	GENERATED_BODY()
	virtual ~FJThread() = default;
	std::shared_ptr<std::jthread> UnderlyingThread;
	FRunnable* UnderylingRunnable;
	virtual FJThread* Create(
	class FRunnable* InRunnable, 
	const TCHAR* ThreadName);
	virtual bool Kill();
	uint32 Run();
};

