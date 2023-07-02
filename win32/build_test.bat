@echo off

set C_FILES=k15_win32_coroutine_test.c
set ASM_FILES=..\arch\x64\k15_cpu_state.asm
set OUTPUT_FILE_NAME=win32_coroutine_test
set LINKER_OPTIONS=/SUBSYSTEM:CONSOLE build\debug\k15_cpu_state.obj
call build_cl.bat

exit /b 0