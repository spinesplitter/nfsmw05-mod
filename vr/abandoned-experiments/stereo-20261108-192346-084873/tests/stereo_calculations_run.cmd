@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
if not exist obj\tests mkdir obj\tests
cl /nologo /EHsc /std:c++17 /MT /O2 /W4 /Foobj\tests\ /Feobj\tests\stereo_calculations_tests.exe tests\stereo_calculations_tests.cpp || exit /b 1
obj\tests\stereo_calculations_tests.exe || exit /b 1
call tests\stereo_visibility_run.cmd || exit /b 1
echo stereo calculation validation passed