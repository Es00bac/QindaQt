#!/bin/sh
# A JVM phone (real NotificationFeature, fake notification source) posts to a real qindaqt-link-service
# on a private D-Bus session bus, with the recording presenter instead of the notification server.
# Usage: phone/engine-tests/run-cross-notify.sh /path/to/qindaqt-link-service
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out="$root/build/phone/engine-tests-n"
rm -rf "$out"; mkdir -p "$out"
javac -d "$out" "$root"/phone/app/android/src/org/qindaqt/link/proto/*.java \
    "$root"/phone/app/android/src/org/qindaqt/link/engine/*.java "$root"/phone/app/android/src/org/qindaqt/link/notify/*.java \
    "$here/SessionTest.java" "$here/CrossImplTest.java" "$here/CrossNotifyTest.java"
exec java -cp "$out" CrossNotifyTest "$1"
