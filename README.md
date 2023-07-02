**Note that this project is currently in development. Features are still missing, stuff is still being build.**

## What does this project try to do? (ELI5)
This project tries to implement [coroutines](https://en.wikipedia.org/wiki/Coroutine) as a portable C library.

## Where can I find the code?
The usage code for the library is in [/win32/k15_win32_coroutine_test.c](https://github.com/FelixK15/k15_coroutine/blob/main/win32/k15_win32_coroutine_test.c).
The code of the actualy library is inside [k15_coroutine.h](https://github.com/FelixK15/k15_coroutine/blob/main/k15_coroutine.h) & [/arch/x64/k15_cpu_state.asm](https://github.com/FelixK15/k15_coroutine/blob/main/arch/x64/k15_cpu_state.asm)

## How do I build this software locally?
Currently the codebase can only be build on a x64 Windows machine with Microsoft Visual Studio installed.
If these requirements are met, simply run the `build_cl.bat`  inside the `win32` directory - This should spawn a `cl.exe` & `ml64.exe` instance which will compile the project and output the executible. In case you want to build a debug version, pass the argument `debug` to the build scripts.

Note: You don't necesseraly *have* to have Visual Studio installed - it's enough if either `cl.exe` and `ml64.exe` are part of your `PATH` environment variable. 
Additionally, the linker has to be able to resolve os library and c stdlib calls.

## How does this work?
The first idea was to build coroutines on top of [fibers](https://en.wikipedia.org/wiki/Fiber_(computer_science)) since they're lightweight and allow easy context-switching without having to spawn full blown threads.
But after reading the excellent [Fibers, Oh My!](https://graphitemaster.github.io/fibers/) blog post by Dale Weiler I realized that this can be implemented without having to use fibers at all.

The way coroutine context switching is implemented is by storing the current cpu state and pre-allocating seperate stacks for each coroutine. When a coroutine is created and should run for the first time, the cpu state of the main thread is safed ("main thread" = the code that passes execution over to the coroutine - technically, coroutines also run on the main thread but for simplification, I'll refer to the coroutine callee as the "main thread") and a pre-allocated stack is assigned to the rsp. Then the arguments for the coroutine get prepared and then the code calls into the coroutine function.

 When a coroutine yields, the current cpu state is saved (that's now the state of the coroutine, basically) and the cpu state of the "main thread" is resolved (this also results in rsp now pointing to the stack of the "main thread").

 Throw in some nasty stack return address patching and, for a first prototype, this actually works.


 **Please note that this is a super bare minimum prototype which is basically just a proof of concept for now**
