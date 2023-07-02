#ifndef K15_COROUTINE_PLATFORM_INCLUDE_GUARD
#error "Please don't include the platform implementation header on your own, these will get included by `k15_coroutine.h`"alignas
#endif

//FK: VirtualProtectFromApp
#pragma comment(lib, "WindowsApp.lib")

#define PAGE_READONLY   0x02    

typedef struct _SYSTEM_INFO {
    union {
        uint32_t dwOemId;          // Obsolete field...do not use
        struct {
            uint16_t wProcessorArchitecture;
            uint16_t wReserved;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
    uint32_t dwPageSize;
    void* lpMinimumApplicationAddress;
    void* lpMaximumApplicationAddress;
    uint32_t* dwActiveProcessorMask;
    uint32_t dwNumberOfProcessors;
    uint32_t dwProcessorType;
    uint32_t dwAllocationGranularity;
    uint16_t wProcessorLevel;
    uint16_t wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;

extern "C"
{
    int __stdcall VirtualProtectFromApp(void*, size_t size, unsigned long newProtection, unsigned long* oldProtection);
    void __stdcall GetSystemInfo(LPSYSTEM_INFO);
}

void _k15_set_page_as_read_only(void* pMemoryPageAddress)
{
    K15_COROUTINE_ASSERT(_k15_is_page_aligned((size_t)pMemoryPageAddress));

    unsigned long oldPageProtection = 0;
    const int result = VirtualProtectFromApp(pMemoryPageAddress, k15_page_size_in_bytes, PAGE_READONLY, &oldPageProtection);
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