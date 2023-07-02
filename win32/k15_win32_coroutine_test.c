#define K15_COROUTINE_IMPLEMENTATION
#include "..\k15_coroutine.h"

#include <stdio.h>

const k15_coroutine_status* coroutine_test(void*)
{
    printf("=> Hello from coroutine start!\n");
    int i = 0;
    
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
    return nullptr;
}

int main()
{
    k15_coroutine_system* pCoroutineSystem = k15_create_coroutine_system();
    k15_register_and_start_coroutine(pCoroutineSystem, coroutine_test);

    while(true)
    {
        k15_update_coroutine_system(pCoroutineSystem);
        printf("Hello from main()!\n");
        Sleep(16);
    }

    k15_destroy_coroutine_system(pCoroutineSystem);
    return 0;
}