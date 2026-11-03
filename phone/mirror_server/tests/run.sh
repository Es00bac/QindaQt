#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Runs the mirror server's pure-Java policy tests (framing, session secret, input policy,
# listen limits) on the host JVM. Plain javac/java, no Android SDK classes, no network.
# Usage: phone/mirror_server/tests/run.sh
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
src="$root/phone/mirror_server/src/main/java/org/qindaqt/link/mirror"
out="$root/build/mirror/mirror-server-tests"
java_home="${JAVA_HOME:-/opt/openjdk-bin-21}"
rm -rf "$out"; mkdir -p "$out"
"$java_home/bin/javac" -Xlint:all -Werror -d "$out" "$src/Frames.java" "$src/SessionSecret.java" \
    "$src/InputPolicy.java" "$src/ListenPolicy.java" "$here/MirrorServerTests.java"
"$java_home/bin/java" -cp "$out" org.qindaqt.link.mirror.MirrorServerTests
