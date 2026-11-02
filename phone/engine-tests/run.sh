#!/bin/sh
# Compiles the pure-Java protocol sources and runs the golden vectors.
# Plain javac/java: no Gradle needed, no network. Usage: phone/engine-tests/run.sh
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out="$root/build/phone/engine-tests"
rm -rf "$out"; mkdir -p "$out"
javac -Xlint:all -d "$out" "$root"/phone/app/android/src/org/qindaqt/link/proto/*.java \
    "$root"/phone/app/android/src/org/qindaqt/link/engine/*.java "$here/VectorTests.java" "$here/SessionTest.java" "$here/RepairTests.java" "$here/FeatureTest.java" "$here/SmsTest.java" "$root"/phone/app/android/src/org/qindaqt/link/sms/*.java 
java -cp "$out" VectorTests "$root/tests/link/vectors"
java -cp "$out" SessionTest
java -cp "$out" RepairTests
java -cp "$out" FeatureTest
java -cp "$out" SmsTest
