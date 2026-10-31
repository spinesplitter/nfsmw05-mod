#!/bin/sh
export PATH='/c/Program Files/Git/cmd':/usr/bin:/mingw32/bin:$PATH
cd '/t/Games/ModWanted/1cookie Need For Speed Most Wanted Black Edition -/vr-source' || exit 1
rm -f kerb-validation.done
sh ffb/tests/run.sh > kerb-ffb-tests.log 2>&1
echo $? > kerb-ffb-tests.exit
sh gameplay/tests/run.sh > kerb-gameplay-tests.log 2>&1
echo $? > kerb-gameplay-tests.exit
sh ffb/build.sh > kerb-ffb-build.log 2>&1
echo $? > kerb-ffb-build.exit
sh gameplay/build.sh > kerb-gameplay-build.log 2>&1
echo $? > kerb-gameplay-build.exit
git diff --check > kerb-diff.log 2>&1
echo $? > kerb-diff.exit
echo complete > kerb-validation.done