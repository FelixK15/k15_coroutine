::version 1

set VCVARS_FILE=vcvars64.bat

if not "%1"=="" (
	set VCVARS_FILE=%1
)

set VCVARS_PATH_CACHE_FILE=.%VCVARS_FILE%.cached_path

::FK: vcvars already run before?
if not "%VisualStudioVersion%"=="" (
	set errorlevel=0
	goto SCRIPT_END
)

if exist "!VCVARS_PATH_CACHE_FILE!" (
	set /p VCVARS_PATH=<!VCVARS_PATH_CACHE_FILE!
	echo Using !VCVARS_FILE! from cache file: !VCVARS_PATH_CACHE_FILE!
	goto EXECUTE_VCVARS_SCRIPT
)

echo Searching for Visual Studio installation...

set FOUND_PATH=0
set VS_PATH=

::check whether this is 64bit windows or not
reg Query "HKLM\Hardware\Description\System\CentralProcessor\0" | find /i "x86" > NUL && set OS=32BIT || set OS=64BIT

if %OS%==64BIT (
	set REG_FOLDER=HKLM\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\SxS\VS7
	set VS_WHERE_PATH="%PROGRAMFILES(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
)

if %OS%==32BIT (
	set REG_FOLDER=HKLM\SOFTWARE\Microsoft\VisualStudio\SxS\VS7
	set VS_WHERE_PATH="%PROGRAMFILES%\Microsoft Visual Studio\Installer\vswhere.exe"
)

::First try to find the visual studio installation via vswhere (AFAIK this is the only way for VS2022 and upward :( )
if exist !VS_WHERE_PATH! (
	set VS_WHERE_COMMAND=!VS_WHERE_PATH! -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
	for /f "delims=" %%i in ('!VS_WHERE_COMMAND!') do set VS_PATH=%%i\

	if "!VS_PATH!"=="" (
		goto PATH_FOUND
	)
	set FOUND_PATH=1
) else (
	::Go to end if nothing was found
	if %REG_FOLDER%=="" goto PATH_FOUND

	::try to get get visual studio path from registry for different versions
	for /l %%G in (20, -1, 8) do (
		set REG_COMMAND=reg query !REG_FOLDER! /v %%G.0
		!REG_COMMAND! >nul 2>nul

		::if errorlevel is 0, we found a valid installDir
		if !errorlevel! == 0 (
			::issue reg command again but evaluate output
			for /F "skip=2 tokens=*" %%A in ('!REG_COMMAND!') do (
				set VS_PATH=%%A
				::truncate stuff we don't want from the output
				set VS_PATH=!VS_PATH:~18!
				set FOUND_PATH=1
				goto PATH_FOUND
			)
		)
	)
)

:PATH_FOUND
::check if a path was found
if !FOUND_PATH!==0 (
	echo Could not find valid Visual Studio installation.
	set errorlevel=2
	goto SCRIPT_END
) else (
	echo Found Visual Studio installation at !VS_PATH!
	set OLD_VCVARS_PATH="!VS_PATH!VC\!VCVARS_FILE!"
	set NEW_VCVARS_PATH="!VS_PATH!VC\Auxiliary\Build\!VCVARS_FILE!"

	if exist !OLD_VCVARS_PATH! (
		set VCVARS_PATH=!OLD_VCVARS_PATH!
		goto STORE_VCVARS_SCRIPT_PATH
	)

	if exist !NEW_VCVARS_PATH! (
		set VCVARS_PATH=!NEW_VCVARS_PATH!
		goto STORE_VCVARS_SCRIPT_PATH
	)

	echo Didn't find !NEW_VCVARS_PATH! or !OLD_VCVARS_PATH! - Does the file exist?
	set errorlevel=1
	goto SCRIPT_END
) 

:STORE_VCVARS_SCRIPT_PATH
if not "!VCVARS_PATH!"=="" (
	echo !VCVARS_PATH! > !VCVARS_PATH_CACHE_FILE!
	attrib !VCVARS_PATH_CACHE_FILE! +h

	echo found !VCVARS_FILE! at !VCVARS_PATH!
)

:EXECUTE_VCVARS_SCRIPT
!VCVARS_PATH!

set errorlevel=0

:SCRIPT_END