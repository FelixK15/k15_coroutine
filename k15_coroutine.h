#ifndef K15_COROUTINE_H
#define K15_COROUTINE_H

#include <stdint.h>

struct k15_coroutine_system;

enum k15_coroutine_system_flags : uint32_t
{
    K15_COROUTINE_SYSTEM_SELF_ALLOCATED_FLAG        = (1<<0),
    K15_COROUTINE_SYSTEM_SET_THREAD_CONTEXT_FLAG    = (1<<1)
};

struct k15_coroutine_handle
{
    uint32_t index;
};

typedef void (*k15_coroutine_function)(void*);

struct k15_coroutine_system_parameter
{
    void*       pMemory;
    size_t      memorySizeInBytes;
    uint32_t    maxCoroutineCount;
    uint32_t    coroutineStackSizeInBytes;
    uint32_t    flags;
};

size_t                          k15_calculate_coroutine_system_memory_requirements(const uint32_t maxCoroutineCount, const uint32_t coroutineStackSizeInBytes);

void*                           k15_malloc_page_aligned(const size_t memorySizeInBytes);
void                            k15_free_page_aligned(void* pMemory);

size_t                          k15_page_align(size_t sizeInBytes);

k15_coroutine_system*           k15_create_coroutine_system();
k15_coroutine_system*           k15_create_coroutine_system_with_parameter(const k15_coroutine_system_parameter* pParameter);
void                            k15_destroy_coroutine_system(k15_coroutine_system* pCoroutineSystem);
void                            k15_update_coroutine_system(k15_coroutine_system* pCoroutineSystem);

k15_coroutine_handle            k15_create_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction);
k15_coroutine_handle            k15_create_coroutine_with_args(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction, void* pArguments, uint64_t argumentSizeInBytes);

k15_coroutine_handle            k15_create_and_resume_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction);
k15_coroutine_handle            k15_create_and_resume_coroutine_with_args(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction, void* pArguments);

void                            k15_resume_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_handle pHandle);
void                            k15_stop_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_handle pHandle);

void                            k15_set_thread_local_coroutine_system(k15_coroutine_system* pCoroutineSystem);

void                            k15_yield();

//Context-less functions
k15_coroutine_handle            k15_create_coroutine(k15_coroutine_function pFunction);
k15_coroutine_handle            k15_create_coroutine_with_args(k15_coroutine_function pFunction, void* pArguments, uint64_t argumentSizeInBytes);

k15_coroutine_handle            k15_create_and_resume_coroutine(k15_coroutine_function pFunction);
k15_coroutine_handle            k15_create_and_resume_coroutine_with_args(k15_coroutine_function pFunction, void* pArguments);

void                            k15_resume_coroutine(k15_coroutine_handle pHandle);
void                            k15_stop_coroutine(k15_coroutine_handle pHandle);


template<typename T>
k15_coroutine_handle            k15_create_coroutine_with_args(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction, T* pArguments)
{
    return k15_create_coroutine_with_args(pCoroutineSystem, pFunction, (void*)pArguments, sizeof(T));
}

#ifdef K15_COROUTINE_IMPLEMENTATION

struct k15_cpu_state
{
    void* rip;
    void* rbx;
    void* rbp;
    void* rdi;
    void* rsi;
    void* rsp;
    void* r12;
    void* r13;
    void* r14;
    void* r15;

    void* xmm6[4];
    void* xmm7[4];
    void* xmm8[4];
    void* xmm9[4];
    void* xmm10[4];
    void* xmm11[4];
    void* xmm12[4];
    void* xmm13[4];
    void* xmm14[4];
    void* xmm15[4];
};

struct k15_coroutine;

//FK: Implemented in k15_cpu_state.asm
extern "C"
{
    extern void     k15_start_coroutine_asm(k15_coroutine* pCoroutine);
    extern void     k15_resume_coroutine_asm(k15_coroutine* pCoroutine);

    extern void     k15_store_cpu_state(k15_cpu_state* pState);
    extern void     k15_apply_cpu_state(const k15_cpu_state* pState);

    extern void*    k15_get_stack_pointer();
    extern void     k15_set_stack_pointer(void* pStackPointer);

    extern void     k15_yield_asm(k15_coroutine* pCoroutine);

    thread_local k15_coroutine*         pThreadLocalActiveCoroutine = nullptr;
    thread_local k15_coroutine_system*  pThreadLocalCoroutineSystem = nullptr;
};

//FK: Implemented by platform header
uint32_t _k15_query_page_size();

#include <malloc.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#ifndef K15_COROUTINE_ERROR_LOG
#include <stdio.h>
#define K15_COROUTINE_ERROR_LOG(msg, ...) printf("%s\n", msg, __VA_ARGS__)
#endif //K15_COROUTINE_ERROR_LOG

#ifndef K15_COROUTINE_ASSERT
#include <assert.h>
#define K15_COROUTINE_ASSERT(cond) assert(cond)
#endif //K15_COROUTINE_ASSERT

#define K15_MAX(a,b) ((a)>(b)?(a):(b))

#define K15_DIVIDE_BY_64(x) ((x)>>6)

constexpr uint64_t k15_default_coroutine_system_memory_size_in_bytes = 1024u*1024u;

enum k15_coroutine_status : uint8_t
{
    K15_COROUTINE_STATUS_SCHEDULED = 0,
    K15_COROUTINE_STATUS_RUNNING,
    K15_COROUTINE_STATUS_DEAD,
    K15_COROUTINE_STATUS_NON_SCHEDULED
};

struct k15_coroutine
{
    k15_coroutine_function          pFunction;
    void*                           pArguments;
    void*                           pStack;
    k15_coroutine*                  pPrevCoroutine;
    k15_cpu_state                   prevCpuState;
    k15_cpu_state                   cpuState;
    k15_coroutine_status            status;
};

struct k15_coroutine_system
{
    k15_coroutine*                          pCoroutines;
    uint64_t*                               pCoroutineUsageMask;
    void*                                   pMemory;
    uint64_t                                memorySizeInBytes;
    uint32_t                                maxCoroutineCount;
    uint32_t                                flags;
};

const uint64_t k15_page_size_in_bytes = _k15_query_page_size();

bool _k15_set_default_coroutine_parameter(k15_coroutine_system_parameter* pOutParameter)
{
    const uint64_t defaultCoroutineCount = 32u;
    const uint64_t defaultCoroutineStackCount = 1024u*1024u;
    const uint64_t requiredMemorySizeInBytes = k15_calculate_coroutine_system_memory_requirements(defaultCoroutineCount, defaultCoroutineStackCount);

    void* pMemory = k15_malloc_page_aligned(requiredMemorySizeInBytes);
    if(pMemory == nullptr)
    {
        return false;
    }

    pOutParameter->pMemory                      = pMemory;
    pOutParameter->maxCoroutineCount            = 32u;
    pOutParameter->coroutineStackSizeInBytes    = 1024u*1024u;
    pOutParameter->memorySizeInBytes            = requiredMemorySizeInBytes;
    pOutParameter->flags                        = K15_COROUTINE_SYSTEM_SELF_ALLOCATED_FLAG | K15_COROUTINE_SYSTEM_SET_THREAD_CONTEXT_FLAG;
    return true;
}

bool _k15_is_page_aligned(size_t value)
{
    return (value % k15_page_size_in_bytes) == 0;
}

bool _k15_coroutine_system_parameter_are_valid(const k15_coroutine_system_parameter* pParameter)
{
    return pParameter->pMemory != nullptr && 
        pParameter->memorySizeInBytes >= k15_default_coroutine_system_memory_size_in_bytes &&
        _k15_is_page_aligned((size_t)pParameter->pMemory) &&
        _k15_is_page_aligned((size_t)pParameter->coroutineStackSizeInBytes) &&
        _k15_is_page_aligned(pParameter->memorySizeInBytes);
}

uint32_t _k15_get_next_coroutine_index(uint64_t* pCoroutineUsageMask, const uint32_t maxCoroutineCount)
{
    const uint32_t maxUsageMaskIndex = K15_DIVIDE_BY_64(maxCoroutineCount);
    const uint32_t maxBitOfLastUsageMask = abs(maxCoroutineCount - maxUsageMaskIndex);

    for(uint32_t usageMaskIndex = 0u; usageMaskIndex < (maxUsageMaskIndex-1u); ++usageMaskIndex)
    {
        uint64_t usageMask = pCoroutineUsageMask[usageMaskIndex];

        for(uint32_t bitIndex = 0; bitIndex < 64u; ++bitIndex)
        {
            const uint64_t bitFlag = (1ull<<bitIndex);
            if((usageMask & bitFlag) == 0)
            {
                usageMask |= bitFlag;

                pCoroutineUsageMask[usageMaskIndex] = usageMask;
                return usageMaskIndex * 64 + bitIndex;
            }
        }
    }

    uint64_t usageMask = pCoroutineUsageMask[maxUsageMaskIndex];
    for(uint32_t bitIndex = 0; bitIndex < maxBitOfLastUsageMask; ++bitIndex)
    {
        const uint64_t bitFlag = (1ull<<bitIndex);
        if((usageMask & bitFlag) == 0)
        {
            usageMask |= bitFlag;

            pCoroutineUsageMask[maxUsageMaskIndex] = usageMask;
            return maxUsageMaskIndex * 64 + bitIndex;
        }
    }

    return UINT32_MAX;
}

uint32_t _k15_create_coroutine(k15_coroutine_system* pSystem, k15_coroutine_function pFunction, void* pArguments, uint64_t argumentSizeInBytes)
{
    uint32_t coroutineIndex = _k15_get_next_coroutine_index(pSystem->pCoroutineUsageMask, pSystem->maxCoroutineCount);
    if(coroutineIndex == UINT64_MAX)
    {
        return UINT32_MAX;
    }

    pSystem->pCoroutines[coroutineIndex].pFunction  = pFunction;
    pSystem->pCoroutines[coroutineIndex].pArguments = pArguments;
    pSystem->pCoroutines[coroutineIndex].status     = K15_COROUTINE_STATUS_NON_SCHEDULED;

    return coroutineIndex;
}

void _k15_resume_coroutine(k15_coroutine* pCoroutine)
{
    pCoroutine->pPrevCoroutine = pThreadLocalActiveCoroutine;
    pThreadLocalActiveCoroutine = pCoroutine;

    k15_resume_coroutine_asm(pCoroutine);

    pThreadLocalActiveCoroutine = pCoroutine->pPrevCoroutine;
}

void _k15_start_coroutine(k15_coroutine* pCoroutine)
{
    pCoroutine->pPrevCoroutine = pThreadLocalActiveCoroutine;
    pThreadLocalActiveCoroutine = pCoroutine;

    k15_start_coroutine_asm(pCoroutine);

    pThreadLocalActiveCoroutine = pCoroutine->pPrevCoroutine;
}

void _k15_schedule_coroutine(k15_coroutine* pCoroutine)
{
    pCoroutine->status = K15_COROUTINE_STATUS_SCHEDULED;
    _k15_start_coroutine(pCoroutine);
}

#define K15_COROUTINE_PLATFORM_INCLUDE_GUARD

#ifdef _WIN32 
#include "win32/k15_win32_page_helper.h"
#else
#error "Platform not supported"
#endif
 
#undef K15_COROUTINE_PLATFORM_INCLUDE_GUARD

size_t k15_calculate_coroutine_system_memory_requirements(const uint32_t maxCoroutineCount, const uint32_t coroutineStackSizeInBytes)
{
    const uint64_t alignedStackSizeInBytes = maxCoroutineCount * k15_page_align(coroutineStackSizeInBytes);
    const uint64_t stackProtectPages = maxCoroutineCount * k15_page_size_in_bytes;
    const uint64_t totalStackMemorySizeInBytes = alignedStackSizeInBytes + stackProtectPages;

    const uint64_t coroutineUsageMaskCount = K15_MAX(1u, K15_DIVIDE_BY_64(maxCoroutineCount));
    const uint64_t systemSizeInBytes = sizeof(k15_coroutine_system) + maxCoroutineCount * sizeof(k15_coroutine) + coroutineUsageMaskCount * sizeof(uint64_t);

    return k15_page_align(totalStackMemorySizeInBytes + systemSizeInBytes);
}

void* k15_malloc_page_aligned(size_t memorySizeInBytes)
{
    memorySizeInBytes = k15_page_align(memorySizeInBytes);
    memorySizeInBytes += sizeof(void*); //FK: to store basepointer
    memorySizeInBytes += k15_page_size_in_bytes;

    uint8_t* pMemory = (uint8_t*)malloc(memorySizeInBytes);
    void* pBaseAddress = pMemory;

    uintptr_t memoryAddress = (uintptr_t)(pMemory + sizeof(void*));

    if((memoryAddress % k15_page_size_in_bytes) != 0u)
    {
        pMemory += k15_page_size_in_bytes-(memoryAddress % k15_page_size_in_bytes) + sizeof(void*);
    }

    size_t* pBasePointer = (size_t*)(pMemory-sizeof(void*));
    *pBasePointer = (size_t)pBaseAddress;
    
    return pMemory;
}

void k15_free_page_aligned(void* pMemory)
{
    size_t* pAlignedMemory = (size_t*)pMemory;
    void* pBasePointer = pAlignedMemory - 1;
    free(pBasePointer);
}

size_t k15_page_align(size_t sizeInBytes)
{
    const size_t rest = (sizeInBytes % k15_page_size_in_bytes);
    if(rest == 0)
    {
        return sizeInBytes;
    }

    return sizeInBytes + k15_page_size_in_bytes - rest;
}

k15_coroutine_system* k15_create_coroutine_system()
{
    k15_coroutine_system_parameter defaultParameter;
    if(!_k15_set_default_coroutine_parameter(&defaultParameter))
    {
        K15_COROUTINE_ERROR_LOG("Out of memory during initializing");
        return nullptr;
    };

    return k15_create_coroutine_system_with_parameter(&defaultParameter);
}

k15_coroutine_system* k15_create_coroutine_system_with_parameter(const k15_coroutine_system_parameter* pParameter)
{
    const uint64_t pageSizeInBytes = _k15_query_page_size();

    K15_COROUTINE_ASSERT(pParameter != nullptr);
    if(!_k15_coroutine_system_parameter_are_valid(pParameter))
    {
        K15_COROUTINE_ERROR_LOG("Invalid coroutine system parameter");
        return nullptr;
    }

    memset(pParameter->pMemory, 0, pParameter->memorySizeInBytes);
    
    const uint64_t coroutineUsageMaskCount = K15_MAX(1u, K15_DIVIDE_BY_64(pParameter->maxCoroutineCount));
    const uint64_t memoryRequirementsInBytes = sizeof(k15_coroutine_system) + 
        pParameter->maxCoroutineCount * sizeof(k15_coroutine) + 
        coroutineUsageMaskCount * sizeof(uint64_t);
    
    if(memoryRequirementsInBytes >= pParameter->memorySizeInBytes)
    {
        K15_COROUTINE_ERROR_LOG("Need more memory");
        return nullptr;
    }

    uint8_t* pMemory = (uint8_t*)pParameter->pMemory;
    k15_coroutine_system* pSystem = (k15_coroutine_system*)pMemory;
    pMemory += sizeof(k15_coroutine_system);

    pSystem->pCoroutines        = (k15_coroutine*)pMemory;
    pMemory += sizeof(k15_coroutine) * pParameter->maxCoroutineCount;

    pSystem->pCoroutineUsageMask    = (uint64_t*)pMemory;
    pMemory += sizeof(uint64_t) * coroutineUsageMaskCount;

    pSystem->maxCoroutineCount  = pParameter->maxCoroutineCount;
    pSystem->pMemory            = pParameter->pMemory;
    pSystem->memorySizeInBytes  = pParameter->memorySizeInBytes;
    pSystem->flags              = pParameter->flags;

    uint8_t* pCoroutineStack = (uint8_t*)(k15_page_align((size_t)pMemory) + pParameter->maxCoroutineCount * pParameter->coroutineStackSizeInBytes);
    for(uint32_t coroutineIndex = 0; coroutineIndex < pParameter->maxCoroutineCount; ++coroutineIndex)
    {
        pSystem->pCoroutines[coroutineIndex].pStack = pCoroutineStack;
        pCoroutineStack -= pParameter->coroutineStackSizeInBytes;
        
        //FK: Put a read-only page before each stack to prevent stack-overflow
        _k15_set_page_as_read_only(pCoroutineStack);
        pCoroutineStack -= k15_page_size_in_bytes;
    }

    if(pParameter->flags & K15_COROUTINE_SYSTEM_SET_THREAD_CONTEXT_FLAG)
    {
        k15_set_thread_local_coroutine_system(pSystem);
    }

    return pSystem;
}

void k15_destroy_coroutine_system(k15_coroutine_system* pSystem)
{
    K15_COROUTINE_ASSERT(pSystem != nullptr);
    K15_COROUTINE_ASSERT(pSystem->pMemory != nullptr);

    if(pSystem->flags & K15_COROUTINE_SYSTEM_SELF_ALLOCATED_FLAG)
    {
        k15_free_page_aligned(pSystem->pMemory);
    }
}

void _k15_activate_coroutine(k15_coroutine* pCoroutine, k15_cpu_state* pOutCpuState)
{
    k15_store_cpu_state(pOutCpuState);

    k15_set_stack_pointer(pCoroutine->pStack);
    k15_apply_cpu_state(&pCoroutine->cpuState);
}

void k15_update_coroutine_system(k15_coroutine_system* pCoroutineSystem)
{
    void* pStackPointer = k15_get_stack_pointer();
    for(uint32_t coroutineIndex = 0; coroutineIndex < pCoroutineSystem->maxCoroutineCount; ++coroutineIndex)
    {
        const uint64_t coroutineMaskBit = (1ull<<coroutineIndex);
        const uint32_t coroutineMaskIndex =  K15_DIVIDE_BY_64(coroutineIndex);

        if(pCoroutineSystem->pCoroutineUsageMask[coroutineMaskIndex] & coroutineMaskBit)
        {
            k15_coroutine* pCoroutine = pCoroutineSystem->pCoroutines + coroutineIndex;
            if(pCoroutine->status == K15_COROUTINE_STATUS_NON_SCHEDULED)
            {
                _k15_schedule_coroutine(pCoroutine);
            }
            else if(pCoroutine->status == K15_COROUTINE_STATUS_SCHEDULED)
            {
                _k15_resume_coroutine(pCoroutine);
            }
        }
    }
}

k15_coroutine_handle k15_create_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction)
{
    return k15_create_coroutine_with_args(pCoroutineSystem, pFunction, nullptr, 0u);
}

k15_coroutine_handle k15_create_coroutine_with_args(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction, void* pArguments, uint64_t argumentSizeInBytes)
{
    k15_coroutine_handle handle = {};
    handle.index = _k15_create_coroutine(pCoroutineSystem, pFunction, pArguments, argumentSizeInBytes);
    return handle;
}

k15_coroutine_handle k15_create_and_resume_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction)
{
    k15_coroutine_handle handle = {};
    handle.index = _k15_create_coroutine(pCoroutineSystem, pFunction, nullptr, 0u);

    k15_resume_coroutine(pCoroutineSystem, handle);

    return handle;
}

k15_coroutine_handle k15_create_and_resume_coroutine_with_args(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction, void* pArguments)
{
    k15_coroutine_handle handle = {};
    handle.index = _k15_create_coroutine(pCoroutineSystem, pFunction, pArguments, 0u);

    k15_resume_coroutine(pCoroutineSystem, handle);

    return handle;
}

bool _k15_is_valid_handle(const uint64_t* pCoroutineMask, const uint32_t maxCoroutineCount, const k15_coroutine_handle handle)
{
    const uint64_t coroutineMaskBit = (1ull<<handle.index);
    const uint32_t coroutineMaskIndex =  K15_DIVIDE_BY_64(handle.index);

    return pCoroutineMask[coroutineMaskIndex] & coroutineMaskBit;
}

void k15_resume_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_handle handle)
{
    K15_COROUTINE_ASSERT(_k15_is_valid_handle(pCoroutineSystem->pCoroutineUsageMask, pCoroutineSystem->maxCoroutineCount, handle));
    k15_coroutine* pCoroutine = &pCoroutineSystem->pCoroutines[handle.index];

    if(pCoroutine->status == K15_COROUTINE_STATUS_NON_SCHEDULED)
    {
        pCoroutine->status = K15_COROUTINE_STATUS_SCHEDULED;
        _k15_start_coroutine(pCoroutine);
    }
    else if(pCoroutine->status == K15_COROUTINE_STATUS_SCHEDULED)
    {
        _k15_resume_coroutine(pCoroutine);
    }
}

void k15_stop_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_handle handle)
{
    K15_COROUTINE_ASSERT(_k15_is_valid_handle(pCoroutineSystem->pCoroutineUsageMask, pCoroutineSystem->maxCoroutineCount, handle));
    k15_coroutine* pCoroutine = &pCoroutineSystem->pCoroutines[handle.index];

    K15_COROUTINE_ASSERT(pCoroutine->status == K15_COROUTINE_STATUS_SCHEDULED);
}

void k15_set_thread_local_coroutine_system(k15_coroutine_system* pCoroutineSystem)
{
    pThreadLocalCoroutineSystem = pCoroutineSystem;
}

void k15_yield()
{
    k15_yield_asm(pThreadLocalActiveCoroutine);
}

k15_coroutine_handle k15_create_coroutine(k15_coroutine_function pFunction)
{
    return k15_create_coroutine(pThreadLocalCoroutineSystem, pFunction);
}

k15_coroutine_handle k15_create_coroutine_with_args(k15_coroutine_function pFunction, void* pArguments, uint64_t argumentSizeInBytes)
{
    return k15_create_coroutine_with_args(pThreadLocalCoroutineSystem, pFunction, pArguments, argumentSizeInBytes);
}

k15_coroutine_handle k15_create_and_resume_coroutine(k15_coroutine_function pFunction)
{
    return k15_create_and_resume_coroutine(pThreadLocalCoroutineSystem, pFunction);
}

k15_coroutine_handle k15_create_and_resume_coroutine_with_args(k15_coroutine_function pFunction, void* pArguments)
{
    return k15_create_and_resume_coroutine_with_args(pThreadLocalCoroutineSystem, pFunction, pArguments);
}

void k15_resume_coroutine(k15_coroutine_handle handle)
{
    k15_resume_coroutine(pThreadLocalCoroutineSystem, handle);
}

void k15_stop_coroutine(k15_coroutine_handle handle)
{
    k15_stop_coroutine(pThreadLocalCoroutineSystem, handle);
}

#endif //K15_COROUTINE_IMPLEMENTATION
#endif //K15_COROUTINE_H