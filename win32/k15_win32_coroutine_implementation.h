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

void _k15_set_page_as_read_only(void* pMemoryPageAddress)
{
    K15_COROUTINE_ASSERT(_k15_is_page_aligned((size_t)pMemoryPageAddress));

    DWORD oldPageProtection = 0;
    const BOOL result = VirtualProtect(pMemoryPageAddress, k15_page_size_in_bytes, PAGE_READONLY, &oldPageProtection);
    if(result == 0)
    {
        K15_COROUTINE_ERROR_LOG("Error during VirtualProtect()");
    }
}

uint32_t _k15_query_page_size()
{
    SYSTEM_INFO systemInfo = {};
    GetSystemInfo(&systemInfo);

    return systemInfo.dwPageSize;
}