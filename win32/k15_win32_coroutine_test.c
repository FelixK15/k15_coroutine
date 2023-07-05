#define K15_COROUTINE_IMPLEMENTATION
#include "..\k15_coroutine.h"

#include <stdio.h>

extern "C"
{
    void Sleep(uint32_t);   
}

int i = 0;
int j = 0;

void coroutine_test(void*)
{
    printf("=> Hello from coroutine start!\n");
    while(true)
    {
        printf("=> local coroutine variable current run: %d\n", i);
        k15_yield();

        if( i++ == 10 )
        {
            break;
        }
    }

    printf("=> Bye from coroutine start!\n");
    return;
}

void nested_inner_coroutine_test(void*)
{
    printf("==> Hello from inner coroutine start!\n");
    k15_yield();
    printf("==> Hello from inner coroutine AGAIN!\n");
    k15_yield();
    printf("==> Bye from inner coroutine...\n");
    
    return;
}

void nested_outer_coroutine_test(void* pArgument)
{
    printf("=> Hello from outer coroutine start!\n");
    k15_coroutine_handle coro = k15_create_and_resume_coroutine(nested_inner_coroutine_test);
    k15_yield();
    printf("=> Back to outer coroutine...\n");
    k15_resume_coroutine(coro);
    k15_yield();
    printf("=> Back to outer coroutine again...\n");
    k15_resume_coroutine(coro);
    k15_yield();
    printf("=> Bye from outer coroutine again...\n");
    return;
}

int main()
{
    k15_coroutine_system* pCoroutineSystem = k15_create_coroutine_system();

#if 0
    k15_coroutine_handle coro = k15_create_coroutines(coroutine_test);

    while(i < 11)
    {
        k15_resume_coroutine(pCoroutineSystem, coro);
        printf("Hello from main()!\n");
        Sleep(16);
    }
#else
    k15_coroutine_handle coro = k15_create_coroutine(nested_outer_coroutine_test);
    k15_resume_coroutine(coro);
    k15_resume_coroutine(coro);
    k15_resume_coroutine(coro);
    k15_resume_coroutine(coro);
    k15_resume_coroutine(coro);
#endif
    return 0;
}