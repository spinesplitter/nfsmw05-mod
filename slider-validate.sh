#!/bin/sh
export PATH=/usr/bin:/mingw32/bin:$PATH
cd '/t/Games/ModWanted/1cookie Need For Speed Most Wanted Black Edition -/vr-source' || exit 1
sh gameplay/tests/run.sh > slider-tests.log 2>&1
echo $? > slider-tests.exit
sh gameplay/build.sh > slider-build.log 2>&1
echo $? > slider-build.exit
git diff --check > slider-diff.log 2>&1
echo $? > slider-diff.exit
echo complete > slider-validation.done