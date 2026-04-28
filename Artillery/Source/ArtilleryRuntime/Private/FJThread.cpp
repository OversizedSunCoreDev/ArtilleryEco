#include "FJThread.h"
THIRD_PARTY_INCLUDES_START
PRAGMA_PUSH_PLATFORM_DEFAULT_PACKING

#include <processthreadsapi.h>

// //we likely should also use the set res function below. if you'd like to add it or it's needed, see:
// //https://github.com/winsiderss/systeminformer/tree/master/phnt
// extern "C" {
// 	NTSYSAPI
// 	NTSTATUS
// 	NTAPI
// 	NtSetTimerResolution(
// 		IN ULONG RequestedResolution,
// 		IN BOOLEAN Set,
// 		OUT PULONG ActualResolution
// 	);
// }

typedef BOOL(WINAPI* PSET_PROCESS_INFORMATION)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);
PRAGMA_POP_PLATFORM_DEFAULT_PACKING
THIRD_PARTY_INCLUDES_END

FJThread* FJThread::Create(class FRunnable* InRunnable, const TCHAR* InThreadName)
{
	bool bCreateRealThread = FPlatformProcess::SupportsMultithreading();

	if (bCreateRealThread)
	{
		// Create a new thread object
		UnderylingRunnable = InRunnable;
		ThreadName = InThreadName;
		UnderlyingThread = std::make_shared<std::jthread>(&FJThread::Run, this);
	}

	if (UnderlyingThread)
	{
		return this;	
	}
	return nullptr;
}

bool FJThread::Kill()
{
	if (UnderylingRunnable)
	{
		UnderylingRunnable->Stop();
	}
	UnderlyingThread.reset(); //jthread ain no slouch.
	return true;
}

uint32 FJThread::Run()
{
	uint32 ExitCode = 1;

	if (UnderylingRunnable && UnderylingRunnable->Init() == true)
	{
		// Give the thread a name so we can see it doing things
		// Incrementing id so that we get unique names (the engine does this as well so I am mostly following along
		static TAtomic<uint32> ThreadCounter(0);
		FString IncrementingThreadName = FString::Printf(TEXT("%s: %d"), *ThreadName, ThreadCounter++);

#if UE_TRACE_ENABLED
		// This lets us see the thread's intended name inside of unreal insights
		UE::Trace::ThreadRegister(*IncrementingThreadName, FPlatformTLS::GetCurrentThreadId(), TPri_Highest);
#endif
		FPlatformProcess::SetThreadName(*IncrementingThreadName);

#if (_WIN32_WINNT >= 0x0602) // UE likes to set things to 0x0601 (Windows 7) by default, but we need >= Windows 8 (0x0602) for power throttling.
		PROCESS_POWER_THROTTLING_STATE PowerThrottling;
		RtlZeroMemory(&PowerThrottling, sizeof(PowerThrottling));
		PowerThrottling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;

		PowerThrottling.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
		PowerThrottling.StateMask = 0;

		SetProcessInformation(GetCurrentProcess(), 
							  ProcessPowerThrottling, 
							  &PowerThrottling,
							  sizeof(PowerThrottling));
	
#endif // _WIN32_WINNT >= 0x0602
		auto hThread = UnderlyingThread->native_handle();
		auto prio = SetThreadPriority(hThread, 0x00000080);//this does something.... odd. enjoy.
		ExitCode = UnderylingRunnable->Run();

		// Allow any allocated resources to be cleaned up
		UnderylingRunnable->Exit();
	}

	return ExitCode;
}
