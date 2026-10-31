#!/bin/sh
# Fresh validation only; never installs binaries or rewrites game configuration.
set -eu
export PATH='/c/Program Files/Git/cmd':/usr/bin:/mingw32/bin:$PATH
cd "$(dirname "$0")"
mkdir -p dist/session-validation
failed=0
for task in ffb/tests/run.sh gameplay/tests/run.sh ffb/build.sh gameplay/build.sh; do
    name=$(printf '%s' "$task" | tr '/' '-')
    result=0
    sh "$task" > "dist/session-validation/$name.log" 2>&1 || result=$?
    printf '%s\n' "$result" > "dist/session-validation/$name.exit"
    [ "$result" -eq 0 ] || failed=1
done
result=0
git diff --check > dist/session-validation/diff.log 2>&1 || result=$?
printf '%s\n' "$result" > dist/session-validation/diff.exit
[ "$result" -eq 0 ] || failed=1
printf '%s\n' "$failed" > dist/session-validation/mingw.done
exit "$failed"