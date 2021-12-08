@echo off
rem This batch file is intended to be used from vim with 'set makeprg=cmd\ /c\ make.bat'
rem You will need to adapt the path below to your environment.

call %MY_VISUAL_STUDIO_IDE_DIR%\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64

set DIR=%~dp0
echo Entering directory %DIR%
cd %DIR%
call build.bat
