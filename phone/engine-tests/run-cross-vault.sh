#!/bin/sh
# A real JVM phone (VaultTypeFeature over JSSE TLS) against the real qindaqt-link-service on a private bus,
# with a fake RemoteDesktop portal and a fake focus publisher (tst_link_vault_cross).
# Usage: phone/engine-tests/run-cross-vault.sh <build dir containing tests/link/qindaqt_link_vault_cross_test>
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out="$root/build/phone/engine-tests-xv"
rm -rf "$out"; mkdir -p "$out"
javac -d "$out" "$root"/phone/app/android/src/org/qindaqt/link/proto/*.java \
    "$root"/phone/app/android/src/org/qindaqt/link/engine/*.java "$root"/phone/app/android/src/org/qindaqt/link/vault/*.java \
    "$here/SessionTest.java" "$here/VaultTests.java" "$here/CrossVaultPhone.java"
QINDALINK_VAULT_PHONE_CP="$out" exec dbus-run-session -- "$1/tests/link/qindaqt_link_vault_cross_test"
