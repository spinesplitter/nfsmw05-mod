@echo off
setlocal
cd /d "%~dp0vr"
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /MT /W3 /D_CRT_SECURE_NO_WARNINGS /Foobj\tests\ /Feobj\tests\rigged_overlay_tests.exe tests\rigged_overlay_tests.cpp > ..\rigged-overlay-tests.log 2>&1
if errorlevel 1 goto failed
obj\tests\rigged_overlay_tests.exe >> ..\rigged-overlay-tests.log 2>&1
if errorlevel 1 goto failed
cl /nologo /EHsc /std:c++17 /MT /W3 /D_CRT_SECURE_NO_WARNINGS /Foobj\tests\ /Feobj\tests\custom_driver_tests.exe tests\custom_driver_tests.cpp >> ..\rigged-overlay-tests.log 2>&1
if errorlevel 1 goto failed
obj\tests\custom_driver_tests.exe >> ..\rigged-overlay-tests.log 2>&1
if errorlevel 1 goto failed
cl /nologo /EHsc /std:c++17 /MT /W3 /Foobj\tests\ /Feobj\tests\cockpit_controls_tests.exe tests\cockpit_controls_tests.cpp >> ..\rigged-overlay-tests.log 2>&1
if errorlevel 1 goto failed
obj\tests\cockpit_controls_tests.exe >> ..\rigged-overlay-tests.log 2>&1
if errorlevel 1 goto failed
cl /nologo /EHsc /std:c++17 /MT /W3 /D_CRT_SECURE_NO_WARNINGS /Foobj\tests\ /Feobj\tests\cockpit_tests.exe tests\cockpit_tests.cpp /link /BASE:0x400000 /DYNAMICBASE:NO user32.lib >> ..\rigged-overlay-tests.log 2>&1
if errorlevel 1 goto failed
obj\tests\cockpit_tests.exe >> ..\rigged-overlay-tests.log 2>&1
if errorlevel 1 goto failed
> ..\rigged-overlay-tests.exit echo 0
call build.cmd > ..\rigged-overlay-build.log 2>&1
if errorlevel 1 goto buildfailed
> ..\rigged-overlay-build.exit echo 0
exit /b 0
:failed
> ..\rigged-overlay-tests.exit echo 1
exit /b 1
:buildfailed
> ..\rigged-overlay-build.exit echo 1
exit /b 1