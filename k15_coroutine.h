#ifndef K15_COROUTINE_H
#define K15_COROUTINE_H

#include <stdint.h>

struct k15_coroutine_system;
struct k15_coroutine_status;

struct k15_coroutine_handle
{
    uint64_t index;
};

typedef const k15_coroutine_status* (*k15_coroutine_function)(void*);

struct k15_coroutine_system_parameter
{
    void*       pMemory;
    uint64_t    memorySizeInBytes;
    uint64_t    maxCoroutineCount;
    uint32_t    flags;
};

k15_coroutine_system*           k15_create_coroutine_system();
k15_coroutine_system*           k15_create_coroutine_system_with_parameter(const k15_coroutine_system_parameter* pParameter);
void                            k15_destroy_coroutine_system(k15_coroutine_system* pCoroutineSystem);
void                            k15_update_coroutine_system(k15_coroutine_system* pCoroutineSystem);

k15_coroutine_handle            k15_register_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction);
k15_coroutine_handle            k15_register_coroutine_with_args(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction, void* pArguments, uint64_t argumentSizeInBytes);

k15_coroutine_handle            k15_register_and_start_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction);
k15_coroutine_handle            k15_register_and_start_coroutine_with_args(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction, void* pArguments);

void                            k15_start_coroutine(k15_coroutine_system* pCoroutineSystem, const k15_coroutine_handle* pHandle);
void                            k15_stop_coroutine(k15_coroutine_system* pCoroutineSystem, const k15_coroutine_handle* pHandle);
void                            k15_unregister_coroutine(k15_coroutine_system* pCoroutineSystem, const k15_coroutine_handle* pHandle);
void                            k15_unregister_and_stop_coroutine(k15_coroutine_system* pCoroutineSystem, const k15_coroutine_handle* pHandle);

const k15_coroutine_status*     k15_yield_coroutine_until_next_frame();
const k15_coroutine_status*     k15_yield_stop_coroutine();

template<typename T>
k15_coroutine_handle            k15_register_coroutine_with_args(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction, T* pArguments)
{
    return k15_register_coroutine_with_args(pCoroutineSystem, pFunction, (void*)pArguments, sizeof(T));
}

#ifdef K15_COROUTINE_IMPLEMENTATION

#include <malloc.h>
#include <stdbool.h>
#include <math.h>

#ifndef K15_COROUTINE_ERROR_LOG
#include <stdio.h>
#define K15_COROUTINE_ERROR_LOG(msg, ...) printf(msg, __VA_ARGS__)
#endif //K15_COROUTINE_ERROR_LOG

#ifndef K15_COROUTINE_ASSERT
#include <assert.h>
#define K15_COROUTINE_ASSERT(cond) assert(cond)
#endif //K15_COROUTINE_ASSERT

#define K15_DIVIDE_BY_64(x) ((x)>>6)

constexpr uint64_t k15_default_coroutine_system_memory_size_in_bytes = 1024u*1024u;
constexpr uint32_t k15_coroutine_system_flag_self_allocated = (1<<0);

struct k15_coroutine_status
{

};

struct k15_coroutine
{
    k15_coroutine_status    status;
    k15_coroutine_function  pFunction;
    void*                   pArguments;
};

struct k15_coroutine_platform_implementation
{
    void* pImplementation;
};

struct k15_coroutine_system
{
    k15_coroutine_platform_implementation   platformImpl;
    k15_coroutine*                          pCoroutines;
    uint64_t*                               pCoroutineUsageMask;
    void*                                   pMemory;
    uint64_t                                memorySizeInBytes;
    uint64_t                                maxCoroutineCount;
    uint32_t                                flags;
};

#define K15_COROUTINE_PLATFORM_INCLUDE_GUARD

#ifdef _WIN32 
#include "win32/k15_win32_coroutine_implementation.h"
#else
#error "Platform not supported"
#endif

#undef K15_COROUTINE_PLATFORM_INCLUDE_GUARD


bool _k15_set_default_coroutine_parameter(k15_coroutine_system_parameter* pOutParameter)
{
    void* pMemory = malloc(k15_default_coroutine_system_memory_size_in_bytes);
    if(pMemory == nullptr)
    {
        return false;
    }

    pOutParameter->pMemory           = pMemory;
    pOutParameter->maxCoroutineCount = 32u;
    pOutParameter->memorySizeInBytes = k15_default_coroutine_system_memory_size_in_bytes;
    pOutParameter->flags             = k15_coroutine_system_flag_self_allocated;
    return true;
}

bool _k15_coroutine_system_parameter_are_valid(const k15_coroutine_system_parameter* pParameter)
{
    return pParameter->pMemory != nullptr && pParameter->memorySizeInBytes >= k15_default_coroutine_system_memory_size_in_bytes;
}

void _k15_destroy_coroutine_system_platform_implementation(k15_coroutine_platform_implementation* pPlatformImplementation)
{

}

uint64_t _k15_get_next_coroutine_index(uint64_t* pCoroutineUsageMask, const uint64_t maxCoroutineCount)
{
    const uint64_t maxUsageMaskIndex = K15_DIVIDE_BY_64(maxCoroutineCount);
    const uint64_t maxBitOfLastUsageMask = abs(maxCoroutineCount - maxUsageMaskIndex);

    for(uint64_t usageMaskIndex = 0u; usageMaskIndex < (maxUsageMaskIndex-1u); ++usageMaskIndex)
    {
        uint64_t usageMask = pCoroutineUsageMask[usageMaskIndex];

        for(uint64_t bitIndex = 0; bitIndex < 64u; ++bitIndex)
        {
            const uint64_t bitFlag = (1u<<bitIndex);
            if((usageMask & bitFlag) == 0)
            {
                usageMask |= bitFlag;

                pCoroutineUsageMask[usageMaskIndex] = usageMask;
                return usageMaskIndex * 64 + bitIndex;
            }
        }
    }

    uint64_t usageMask = pCoroutineUsageMask[maxUsageMaskIndex];
    for(uint64_t bitIndex = 0; bitIndex < maxBitOfLastUsageMask; ++bitIndex)
    {
        const uint64_t bitFlag = (1u<<bitIndex);
        if((usageMask & bitFlag) == 0)
        {
            usageMask |= bitFlag;

            pCoroutineUsageMask[maxUsageMaskIndex] = usageMask;
            return maxUsageMaskIndex * 64 + bitIndex;
        }
    }

    return UINT64_MAX;
}

uint64_t _k15_register_coroutine(k15_coroutine_system* pSystem, k15_coroutine_function pFunction, void* pArguments, uint64_t argumentSizeInBytes)
{
    uint64_t coroutineIndex = _k15_get_next_coroutine_index(pSystem->pCoroutineUsageMask, pSystem->maxCoroutineCount);
    if(coroutineIndex == UINT64_MAX)
    {
        return UINT64_MAX;
    }

    pSystem->pCoroutines[coroutineIndex].pFunction  = pFunction;
    pSystem->pCoroutines[coroutineIndex].pArguments = pArguments;
    return coroutineIndex;
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
    K15_COROUTINE_ASSERT(pParameter != nullptr);

    if(!_k15_coroutine_system_parameter_are_valid(pParameter))
    {
        K15_COROUTINE_ERROR_LOG("Invalid coroutine system parameter");
        return nullptr;
    }

    memset(pParameter->pMemory, 0, pParameter->memorySizeInBytes);
    
    const uint64_t coroutineUsageMaskCount = max(1u, K15_DIVIDE_BY_64(pParameter->maxCoroutineCount));
    const uint64_t memoryRequirementsInBytes = sizeof(k15_coroutine_system) + 
        pParameter->maxCoroutineCount * sizeof(k15_coroutine) + 
        coroutineUsageMaskCount * sizeof(uint64_t) +
        _k15_get_size_of_coroutine_platform_implementation();
    
    if(memoryRequirementsInBytes >= pParameter->memorySizeInBytes)
    {
        K15_COROUTINE_ERROR_LOG("Need more memory");
        return nullptr;
    }

    uint8_t* pMemory = (uint8_t*)pParameter->pMemory;
    size_t memorySizeInBytes = pParameter->memorySizeInBytes;
    k15_coroutine_system* pSystem = (k15_coroutine_system*)pMemory;
    pMemory += sizeof(k15_coroutine_system);
    memorySizeInBytes -= sizeof(k15_coroutine_system);

    pSystem->pCoroutines        = (k15_coroutine*)pMemory;
    pMemory += sizeof(k15_coroutine) * pParameter->maxCoroutineCount;
    memorySizeInBytes -= sizeof(k15_coroutine) * pParameter->maxCoroutineCount;

    pSystem->pCoroutineUsageMask    = (uint64_t*)pMemory;
    pMemory += sizeof(uint64_t) * coroutineUsageMaskCount;
    memorySizeInBytes -= sizeof(uint64_t) * coroutineUsageMaskCount;  

    pSystem->maxCoroutineCount  = pParameter->maxCoroutineCount;
    pSystem->pMemory            = pParameter->pMemory;
    pSystem->memorySizeInBytes  = pParameter->memorySizeInBytes;
    pSystem->flags              = pParameter->flags;

    if(!_k15_create_coroutine_system_platform_implementation(&pSystem->platformImpl, pMemory, memorySizeInBytes))
    {
        K15_COROUTINE_ERROR_LOG("Error during platform implementation creation");
        k15_destroy_coroutine_system(pSystem);
        return nullptr;
    }

    return pSystem;
}

void k15_destroy_coroutine_system(k15_coroutine_system* pSystem)
{
    K15_COROUTINE_ASSERT(pSystem != nullptr);
    K15_COROUTINE_ASSERT(pSystem->pMemory != nullptr);

    _k15_destroy_coroutine_system_platform_implementation(&pSystem->platformImpl);

    if(pSystem->flags & k15_coroutine_system_flag_self_allocated)
    {
        free(pSystem->pMemory);
    }
}

void k15_update_coroutine_system(k15_coroutine_system* pCoroutineSystem)
{

}

k15_coroutine_handle k15_register_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction)
{
    return k15_register_coroutine_with_args(pCoroutineSystem, pFunction, nullptr, 0u);
}

k15_coroutine_handle k15_register_coroutine_with_args(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction, void* pArguments, uint64_t argumentSizeInBytes)
{
    k15_coroutine_handle handle = {};
    handle.index = _k15_register_coroutine(pCoroutineSystem, pFunction, pArguments, argumentSizeInBytes);
    return handle;
}

k15_coroutine_handle k15_register_and_start_coroutine(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction)
{
    k15_coroutine_handle handle = {};
    return handle;
}

k15_coroutine_handle k15_register_and_start_coroutine_with_args(k15_coroutine_system* pCoroutineSystem, k15_coroutine_function pFunction, void* pArguments)
{
    k15_coroutine_handle handle = {};
    return handle;
}

void k15_start_coroutine(k15_coroutine_system* pCoroutineSystem, const k15_coroutine_handle* pHandle)
{

}

void k15_stop_coroutine(k15_coroutine_system* pCoroutineSystem, const k15_coroutine_handle* pHandle)
{

}

void k15_unregister_coroutine(k15_coroutine_system* pCoroutineSystem, const k15_coroutine_handle* pHandle)
{

}

void k15_unregister_and_stop_coroutine(k15_coroutine_system* pCoroutineSystem, const k15_coroutine_handle* pHandle)
{

}

const k15_coroutine_status* k15_yield_coroutine_until_next_frame()
{
    return nullptr;
}

const k15_coroutine_status* k15_stop_coroutine()
{
    return nullptr;
}

#endif //K15_COROUTINE_IMPLEMENTATION
#endif //K15_COROUTINE_H