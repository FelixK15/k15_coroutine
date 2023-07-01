@echo off

set C_FILES=k15_win32_coroutine_test.c
set OUTPUT_FILE_NAME=win32_coroutine_test
set LINKER_OPTIONS=/SUBSYSTEM:CONSOLE
call build_cl.bat
exit /b 0