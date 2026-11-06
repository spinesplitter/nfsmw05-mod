@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 exit /b 1
if not exist obj\tests mkdir obj\tests
cl /nologo /EHsc /std:c++17 /MT /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Foobj\tests\ /Feobj\tests\solid_cache_tests.exe tests\solid_cache_tests.cpp /link /BASE:0x400000 /DYNAMICBASE:NO user32.lib
if errorlevel 1 exit /b 1
obj\tests\solid_cache_tests.exe
exit /b %ERRORLEVEL%