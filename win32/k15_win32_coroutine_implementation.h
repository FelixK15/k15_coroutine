#ifndef K15_COROUTINE_PLATFORM_INCLUDE_GUARD
#error "Please don't include the platform implementation header on your own, these will get included by `k15_coroutine.h`"alignas
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct k15_win32_coroutine_platform_implementation
{
    HANDLE  pCoroutineThread;
};

DWORD WINAPI _k15_win32_coroutine_thread_entry_point(LPVOID pParameter)
{
    return 0u;
}

uint64_t _k15_get_size_of_coroutine_platform_implementation()
{
    return sizeof(k15_win32_coroutine_platform_implementation);
}

bool _k15_create_coroutine_system_platform_implementation(k15_coroutine_platform_implementation* pOutPlatformImplementation, void* pMemory, uint64_t memorySizeInBytes)
{
    uint8_t* pMemoryBlock = (uint8_t*)pMemory;

    k15_win32_coroutine_platform_implementation* pWin32Implementation = (k15_win32_coroutine_platform_implementation*)pMemoryBlock;
    pMemoryBlock += sizeof(k15_win32_coroutine_platform_implementation);
    memorySizeInBytes -= sizeof(k15_win32_coroutine_platform_implementation);

    constexpr size_t threadStackSizeInBytes = 1024*1024;
    HANDLE pCoroutineThread = CreateThread(nullptr, threadStackSizeInBytes, _k15_win32_coroutine_thread_entry_point, nullptr, CREATE_SUSPENDED, nullptr);
    if(pCoroutineThread == nullptr)
    {
        return false;
    }

    return true;
}