#!/bin/sh
# Real phone engine over the LAN path vs a real qindaqt-link-service on a private D-Bus session bus (127.0.0.2).
# Usage: phone/engine-tests/run-cross-lan.sh /path/to/qindaqt-link-service
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out="$root/build/phone/engine-tests-xl"
rm -rf "$out"; mkdir -p "$out"
javac -d "$out" "$root"/phone/app/android/src/org/qindaqt/link/proto/*.java \
    "$root"/phone/app/android/src/org/qindaqt/link/engine/*.java "$here/SessionTest.java" "$here/CrossImplTest.java" "$here/CrossLanTest.java"
exec java -cp "$out" CrossLanTest "$1"
