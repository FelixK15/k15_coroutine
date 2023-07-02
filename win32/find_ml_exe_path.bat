::version 1

call find_vcvars_path.bat

if !errorlevel! == 0 (
	::FK: Get path of cl.exe
	for /f "delims=" %%i in ('where ml64.exe') do set ASSEMBLER_PATH=%%i
)