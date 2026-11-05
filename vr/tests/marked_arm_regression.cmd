@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul || exit /b 1
call tests\marked_arm_run.cmd || exit /b 1
for %%t in (driver_ik_tests cockpit_controls_tests cockpit_tests hook_tests) do (
  cl /nologo /EHsc /std:c++17 /MT /W3 /D_CRT_SECURE_NO_WARNINGS /Foobj\tests\ /Feobj\tests\%%t.exe tests\%%t.cpp /link /BASE:0x400000 /DYNAMICBASE:NO user32.lib || exit /b 1
  obj\tests\%%t.exe || exit /b 1
)
if not "%~1"=="" obj\tests\driver_ik_tests.exe "%~1" || exit /b 1
echo MARKED ARM REGRESSIONS PASSED