@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
if not exist obj\tests mkdir obj\tests
cl /nologo /EHsc /std:c++17 /MT /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Foobj\tests\ /Feobj\tests\frametrace_detail_tests.exe tests\frametrace_detail_tests.cpp user32.lib || exit /b 1
obj\tests\frametrace_detail_tests.exe || exit /b 1
echo SKIPPED: custom_driver_tests requires absent optional drivermod/driver-runtime.bin asset
call tests\optimization_available_tests.cmd || exit /b 1
call tests\solid_cache_run.cmd || exit /b 1
call build.cmd || exit /b 1
