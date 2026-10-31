#!/bin/sh
export PATH='/c/Program Files/Git/cmd':/usr/bin:/mingw32/bin:$PATH
cd '/t/Games/ModWanted/1cookie Need For Speed Most Wanted Black Edition -/vr-source' || exit 1
sh gameplay/tests/run.sh > opponent-damage-tests.log 2>&1
echo $? > opponent-damage-tests.exit
sh gameplay/build.sh > opponent-damage-build.log 2>&1
echo $? > opponent-damage-build.exit
git diff --check > opponent-damage-diff.log 2>&1
echo $? > opponent-damage-diff.exit