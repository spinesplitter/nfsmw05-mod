@echo off
setlocal
cd /d "%~dp0"
call vr\tests\run.cmd > fps-regression-tests.log 2>&1
set T=%errorlevel%
echo %T% > fps-regression-tests.exit
if not "%T%"=="0" exit /b %T%
call vr\build.cmd > fps-regression-build.log 2>&1
set B=%errorlevel%
echo %B% > fps-regression-build.exit
exit /b %B%