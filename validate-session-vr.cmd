@echo off
setlocal
cd /d "%~dp0"
if not exist dist\session-validation mkdir dist\session-validation
call vr\tests\run.cmd > dist\session-validation\vr-tests.log 2>&1
set TEST_RESULT=%errorlevel%
echo %TEST_RESULT% > dist\session-validation\vr-tests.exit
call vr\build.cmd > dist\session-validation\vr-build.log 2>&1
set BUILD_RESULT=%errorlevel%
echo %BUILD_RESULT% > dist\session-validation\vr-build.exit
if not "%TEST_RESULT%"=="0" goto failed
if not "%BUILD_RESULT%"=="0" goto failed
echo 0 > dist\session-validation\vr.done
exit /b 0
:failed
echo 1 > dist\session-validation\vr.done
exit /b 1