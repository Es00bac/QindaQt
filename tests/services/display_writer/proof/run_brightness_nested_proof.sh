#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# ADR-0150 private nested-KWin brightness proof. It never touches the host
# display, session bus, or hardware:
#
# 1. it re-executes itself with an empty environment inside dbus-run-session,
#    with disposable XDG roots and no system bus;
# 2. starts kwin_wayland --virtual with two outputs on a private socket;
# 3. records the production writer's device facts and local refusals;
# 4. records what KWin itself does with the requests the writer refuses
#    (non-capable, out-of-scale, and disabled targets) through a raw client;
# 5. stops that compositor by its own PID, restarts one on the same socket
#    name, and records the stale owner-generation and identity fences.
#
# Required: QQ_PROOF_DIR (artifacts) and QQ_BUILD_ROOT (built helper).
set -u

PROOF_DIR="${QQ_PROOF_DIR:?QQ_PROOF_DIR is required}"
BUILD_ROOT="${QQ_BUILD_ROOT:?QQ_BUILD_ROOT is required}"

if [ -z "${QQ_D7_PRIVATE:-}" ]; then
    mkdir -p "${PROOF_DIR}/home"
    exec env -i PATH="${PATH}" HOME="${PROOF_DIR}/home" LANG=C.UTF-8 \
        QQ_PROOF_DIR="${PROOF_DIR}" QQ_BUILD_ROOT="${BUILD_ROOT}" QQ_D7_PRIVATE=1 \
        DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent \
        dbus-run-session -- bash "$0"
fi

HELPER="${BUILD_ROOT}/tests/services/display_writer/qindaqt_display_brightness_nested_proof_helper"
SOCKET_NAME="qq-d7-brightness"
FAILURES=0
KWIN_PID=""

# A Unix socket path must fit in 108 bytes, so only the runtime root lives in
# a short private temporary directory; it is removed on exit.
RUNTIME="$(mktemp -d "${TMPDIR:-/tmp}/qq-d7-XXXXXX")"
chmod 700 "${RUNTIME}"
export XDG_RUNTIME_DIR="${RUNTIME}"
export XDG_CONFIG_HOME="${PROOF_DIR}/config"
export XDG_DATA_HOME="${PROOF_DIR}/data"
export XDG_CACHE_HOME="${PROOF_DIR}/cache"
export XDG_STATE_HOME="${PROOF_DIR}/state"
mkdir -p "${XDG_CONFIG_HOME}" "${XDG_DATA_HOME}" "${XDG_CACHE_HOME}" "${XDG_STATE_HOME}"
# Keep any installed QindaQt compositor plugin out of this disposable KWin.
printf '[Plugins]\nqindaqt_compositorEnabled=false\n' > "${XDG_CONFIG_HOME}/kwinrc"

log() { echo "[proof] $*" | tee -a "${PROOF_DIR}/summary.txt"; }
fail() { log "FAIL: $*"; FAILURES=$((FAILURES + 1)); }
pass() { log "PASS: $*"; }

expect() { # file, fixed text, description
    if grep -F -q -- "$2" "${PROOF_DIR}/$1"; then pass "$3"; else fail "$3 (missing '$2' in $1)"; fi
}
expect_none() { # file, extended regex, description
    if grep -E -q -- "$2" "${PROOF_DIR}/$1"; then fail "$3 (found /$2/ in $1)"; else pass "$3"; fi
}

start_kwin() {
    kwin_wayland --virtual --socket "${SOCKET_NAME}" --width 1280 --height 800 \
        --output-count 2 --no-lockscreen --no-global-shortcuts \
        > "${PROOF_DIR}/kwin-$1.log" 2>&1 &
    KWIN_PID=$!
    for _ in $(seq 1 100); do
        [ -S "${XDG_RUNTIME_DIR}/${SOCKET_NAME}" ] && return 0
        kill -0 "${KWIN_PID}" 2>/dev/null || break
        sleep 0.1
    done
    fail "kwin_wayland $1 did not create ${SOCKET_NAME}"
    return 1
}

stop_kwin() {
    [ -n "${KWIN_PID}" ] || return 0
    kill -TERM "${KWIN_PID}" 2>/dev/null
    wait "${KWIN_PID}" 2>/dev/null
    KWIN_PID=""
    rm -f "${XDG_RUNTIME_DIR}/${SOCKET_NAME}" "${XDG_RUNTIME_DIR}/${SOCKET_NAME}.lock"
}
trap 'stop_kwin; rm -rf "${RUNTIME}"' EXIT

: > "${PROOF_DIR}/summary.txt"
log "kwin: $(kwin_wayland --version 2>/dev/null | head -1)"
[ -x "${HELPER}" ] || { fail "helper missing at ${HELPER}"; exit 1; }
export WAYLAND_DISPLAY="${SOCKET_NAME}"

start_kwin first || exit 1
"${HELPER}" observe > "${PROOF_DIR}/observe.txt" 2>&1
log "observe exit $?"
"${HELPER}" raw > "${PROOF_DIR}/raw.txt" 2>&1
log "raw exit $?"
"${HELPER}" hold "${PROOF_DIR}/identity.txt" > "${PROOF_DIR}/hold.txt" 2>&1 &
HELPER_PID=$!
for _ in $(seq 1 100); do
    grep -q '^hold.ready$' "${PROOF_DIR}/hold.txt" 2>/dev/null && break
    sleep 0.1
done
stop_kwin
wait "${HELPER_PID}"
log "hold exit $?"
start_kwin second || exit 1
"${HELPER}" reuse "${PROOF_DIR}/identity.txt" > "${PROOF_DIR}/reuse.txt" 2>&1
log "reuse exit $?"
stop_kwin

expect observe.txt "observe.generation=" "writer published a device frame"
expect_none observe.txt "observe\.generation=0 " "published frame has live owner authority"
expect_none observe.txt "observe\.device .*capable=1" "no virtual output is published brightness-capable"
expect observe.txt "observed=1 value=10000" "writer observes KWin's brightness value"
expect_none observe.txt "observe\.request .*status=(accepted|busy|malformed|unavailable)" "writer refuses every non-capable output locally"
expect observe.txt "observe.request name=Virtual-1 status=unsupported" "writer refusal is typed unsupported"
expect observe.txt "observe.stale-generation status=unavailable" "stale owner generation is unavailable"
expect observe.txt "observe.completions=0" "no refused request completes"
expect raw.txt "raw.management version=19" "raw client binds management v19"
expect_none raw.txt "raw\.device .*capability-brightness=1" "KWin advertises no brightness capability on virtual outputs"
expect raw.txt 'raw.non-capable name=Virtual-0 applied=1 failed=0 reason="" brightness-before=10000 brightness-after=10000 brightness-events=0 enabled=1' "KWin acknowledges set_brightness on a non-capable output and publishes no change"
expect raw.txt 'raw.out-of-scale name=Virtual-0 applied=1 failed=0 reason="" brightness-before=10000 brightness-after=10000 brightness-events=0 enabled=1' "KWin acknowledges an out-of-scale set_brightness and publishes no change"
expect raw.txt "raw.disable name=Virtual-1 applied=1 failed=0" "second output disabled"
expect raw.txt 'raw.disabled name=Virtual-1 applied=1 failed=0 reason="" brightness-before=10000 brightness-after=10000 brightness-events=0 enabled=0' "KWin acknowledges set_brightness on a disabled output and publishes no change"
expect hold.txt "hold.ready" "writer bound before compositor loss"
expect hold.txt "hold.after-loss.generation=0 devices=0" "compositor loss clears device authority"
expect hold.txt "hold.after-loss.request status=unavailable" "request pinned to the lost owner is unavailable"
expect hold.txt "hold.after-loss.available=0" "writer withdraws mutation authority after compositor loss"
# Recorded upstream fact, not a QindaQt fence: a virtual output keeps its
# runtime UUID across a restart, so restart is fenced by the writer's owner
# generation and Display1's new epoch, never by the UUID alone.
expect reuse.txt "reuse.prior-uuid-present=1" "KWin keeps a virtual output runtime UUID across compositor restart"
expect reuse.txt "reuse.device name=Virtual-1 uuid=" "restarted compositor republishes the device set"

log "failures=${FAILURES}"
exit $(( FAILURES == 0 ? 0 : 1 ))
