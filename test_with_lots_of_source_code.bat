@echo off
setlocal EnableDelayedExpansion

REM If you change LIST_FILE, also change the filename in the FOR below. :(
set LIST_FILE="d:\temp\whereami_test_files.txt"
set OUTPUT_FILE="d:\temp\whereami_test_output.txt"

c:\cygwin\bin\find c:\cygwin\home\edwin\git\pdf_analyzer d:\third_party_source -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" >%LIST_FILE%
if ERRORLEVEL 1 goto find_failed

REM using a variable for the list file here unfortunately does not work
FOR /f "delims=" %%G IN (d:\temp\whereami_test_files.txt) DO (^
    echo Testing with file: %%G
    whereami_debug.exe %%G 0 >%OUTPUT_FILE% 2>&1
    if ERRORLEVEL 1 goto error
    whereami.exe %%G 0 >%OUTPUT_FILE% 2>&1
    if ERRORLEVEL 1 goto error
    whereami_nowin32.exe %%G 0 >%OUTPUT_FILE% 2>&1
    if ERRORLEVEL 1 goto error
)

echo OK
goto :done
:error
echo ERROR: whereami failed on last file
goto :done
:find_failed
echo ERROR: find command failed
goto :done
:done
