@echo off
set PATH=C:\msys64\usr\bin;C:\msys64\mingw32\bin;%PATH%
cd /d "t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\vr-source\gameplay"
C:\msys64\usr\bin\bash.exe tests/run.sh > "t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\vr-source\mirror-fix-gameplay-tests.log" 2>&1
echo %errorlevel% > "t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\vr-source\mirror-fix-gameplay-tests.exit"
C:\msys64\usr\bin\bash.exe build.sh > "t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\vr-source\mirror-fix-gameplay-build.log" 2>&1
echo %errorlevel% > "t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\vr-source\mirror-fix-gameplay-build.exit"

