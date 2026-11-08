@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
if not exist obj\tests mkdir obj\tests
cl /nologo /EHsc /std:c++17 /MT /O2 /W3 /Foobj\tests\ /Feobj\tests\stereo_visibility_tests.exe tests\stereo_visibility_tests.cpp || exit /b 1
obj\tests\stereo_visibility_tests.exe || exit /b 1