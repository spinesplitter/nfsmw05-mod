@echo off
cd /d "t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\vr-source\vr"
call tests\run.cmd > "t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\vr-source\mirror-fix-vr-tests.log" 2>&1
echo %errorlevel% > "t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\vr-source\mirror-fix-vr-tests.exit"
call build.cmd > "t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\vr-source\mirror-fix-vr-build.log" 2>&1
echo %errorlevel% > "t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\vr-source\mirror-fix-vr-build.exit"

