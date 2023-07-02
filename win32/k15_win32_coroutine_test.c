#define K15_COROUTINE_IMPLEMENTATION
#include "..\k15_coroutine.h"

#include <stdio.h>

extern "C"
{
    void Sleep(uint32_t);   
}

int i = 0;

void coroutine_test(void*)
{
    printf("=> Hello from coroutine start!\n");
    while(true)
    {
        printf("=> local coroutine variable current run: %d\n", i);
        k15_yield_coroutine_until_next_frame();

        if( i++ == 10 )
        {
            break;
        }
    }

    printf("=> Bye from coroutine start!\n");
    return;
}

int main()
{
    k15_coroutine_system* pCoroutineSystem = k15_create_coroutine_system();
    k15_register_and_start_coroutine(pCoroutineSystem, coroutine_test);

    while(i < 11)
    {
        k15_update_coroutine_system(pCoroutineSystem);
        printf("Hello from main()!\n");
        Sleep(16);
    }

    return 0;
}