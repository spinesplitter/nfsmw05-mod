@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
if not exist obj\tests mkdir obj\tests
cl /nologo /EHsc /std:c++17 /MT /W3 /Foobj\tests\ /Feobj\tests\driver_skin_state_tests.exe tests\driver_skin_state_tests.cpp || exit /b 1
obj\tests\driver_skin_state_tests.exe || exit /b 1
cl /nologo /EHsc /std:c++17 /MT /W3 /D_CRT_SECURE_NO_WARNINGS /Foobj\tests\ /Feobj\tests\skin_picker_tests.exe tests\skin_picker_tests.cpp /link /BASE:0x400000 /DYNAMICBASE:NO user32.lib || exit /b 1
obj\tests\skin_picker_tests.exe || exit /b 1
python tests\mirror_depth_source_tests.py || exit /b 1