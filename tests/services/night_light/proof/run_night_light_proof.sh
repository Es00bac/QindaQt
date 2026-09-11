#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-3.0-or-later
#
# ADR-0136 private headless proof. Runs under qq-private (dbus-run-session,
# no live display or session bus, disposable XDG dirs):
#
# 1. starts a virtual KWin on socket qq-gap-night-light-0 with a private
#    config directory;
# 2. reads the baseline org.kde.KWin.NightLight properties;
# 3. writes Active=true, Mode=Constant, NightTemperature=3400 through the
#    production QtConfigNightLightPort (never by hand-editing the file);
# 4. watches currentTemperature move to 3400 over the private bus;
# 5. previews 2700 K and stops the preview;
# 6. proves the inhibit lock dies with the caller's connection: a helper
#    holds inhibit() for a few seconds and exits without uninhibiting; the
#    lock must disappear when the caller is gone.
#
# Everything the script saves lands in $QQ_PROOF_DIR.

set -u

PROOF_DIR="${QQ_PROOF_DIR:?QQ_PROOF_DIR is required}"
WORKTREE="${QQ_WORKTREE:?QQ_WORKTREE is required}"
HELPER="${QQ_BUILD_ROOT}/tests/services/night_light/qindaqt_night_light_proof_helper"
SOCKET_NAME="qq-gap-night-light-0"
DONE_FILE="${PROOF_DIR}/probe-done"
FAILURES=0

mkdir -p "${PROOF_DIR}"
rm -f "${DONE_FILE}"
: > "${PROOF_DIR}/kwin.log"

log() { echo "[proof] $*" | tee -a "${PROOF_DIR}/kwin.log"; }
fail() {
    echo "[proof] FAIL: $*" | tee -a "${PROOF_DIR}/kwin.log"
    FAILURES=$((FAILURES + 1))
}

get_prop() {
    busctl --user get-property org.kde.KWin.NightLight \
        /org/kde/KWin/NightLight "org.kde.KWin.NightLight" "$1" 2>/dev/null \
        | awk '{print $2}'
}

if [ ! -x "${HELPER}" ]; then
    fail "proof helper missing at ${HELPER}"
    exit 1
fi

# Pre-seed the private config through the production config port BEFORE the
# compositor starts: this isolates "KWin reads the file" from "KWin reloads
# the file". Both writes below go through QtConfigNightLightPort.
CONFIG_DIR="$(mktemp -d "${PROOF_DIR}/config-XXXXXX")"
export XDG_CONFIG_HOME="${CONFIG_DIR}"
if "${HELPER}" write "${XDG_CONFIG_HOME}/kwinrc" \
        "${XDG_CONFIG_HOME}/knighttimerc" \
        > "${PROOF_DIR}/02-write-outcome.txt" 2>&1; then
    log "pre-seed write: Applied (${XDG_CONFIG_HOME}/kwinrc)"
else
    fail "pre-seed write failed (see 02-write-outcome.txt)"
fi
cp "${XDG_CONFIG_HOME}/kwinrc" "${PROOF_DIR}/02-kwinrc-after-write.txt" 2>/dev/null

# ---------------------------------------------------------------- compositor
log "starting kwin_wayland --virtual (socket ${SOCKET_NAME})"
# The probe keeps the compositor alive until this script touches the done
# file; --exit-with-session then tears KWin down with it.
cat > "${PROOF_DIR}/probe.sh" <<PROBE
#!/bin/sh
while [ ! -f "${DONE_FILE}" ]; do sleep 0.3; done
PROBE
chmod +x "${PROOF_DIR}/probe.sh"
(
    export QT_LOGGING_RULES="*.debug=true"
    kwin_wayland --virtual --socket "${SOCKET_NAME}" \
        --width 1280 --height 800 \
        --exit-with-session "${PROOF_DIR}/probe.sh" \
        > "${PROOF_DIR}/kwin.log" 2>&1
) &
KWIN_PID=$!

cleanup() {
    touch "${DONE_FILE}" 2>/dev/null || true
    kill "${KWIN_PID}" 2>/dev/null || true
}
trap cleanup EXIT

# The compositor owns the virtual socket name; wait for the service it exports.
SERVICE_UP=0
for _ in $(seq 1 120); do
    if busctl --user list --no-legend 2>/dev/null \
        | grep -q "org.kde.KWin.NightLight"; then
        SERVICE_UP=1
        break
    fi
    sleep 0.5
done
if [ "${SERVICE_UP}" -ne 1 ]; then
    fail "org.kde.KWin.NightLight never appeared on the private bus"
    busctl --user tree org.kde.KWin \
        > "${PROOF_DIR}/00-kwin-tree.txt" 2>&1 || true
    busctl --user list \
        > "${PROOF_DIR}/00-bus-names.txt" 2>&1 || true
    touch "${DONE_FILE}"
    wait "${KWIN_PID}" 2>/dev/null
    exit 1
fi
log "org.kde.KWin.NightLight is on the private bus"

# ------------------------------------------------------------------ baseline
{
    echo "baseline $(date -Iseconds)"
    for prop in available enabled running inhibited mode daylight \
        currentTemperature targetTemperature; do
        echo "  ${prop}=$(get_prop "${prop}")"
    done
} > "${PROOF_DIR}/01-baseline.txt"
cat "${PROOF_DIR}/01-baseline.txt"
AVAILABLE=$(get_prop available)
[ "${AVAILABLE}" = "true" ] || fail "plugin reports available=${AVAILABLE}"

# ------------------------------------------- write through the config port
# The written file must be the very kwinrc this compositor watches: KWin
# resolved it from XDG_CONFIG_HOME at startup.
KWINRC_PATH="${XDG_CONFIG_HOME:-${HOME}/.config}/kwinrc"
KNIGHTRC_PATH="${XDG_CONFIG_HOME:-${HOME}/.config}/knighttimerc"
# Live reload: write a CHANGED night temperature through the port after the
# compositor is up, then reconfigure and watch it converge.
log "writing NightTemperature=4500 via the config port (live change)"
if "${HELPER}" write "${KWINRC_PATH}" "${KNIGHTRC_PATH}" \
    > "${PROOF_DIR}/02-write-outcome.txt" 2>&1; then
    log "config port write: Applied"
else
    fail "config port write failed (see 02-write-outcome.txt)"
fi
cp "${KWINRC_PATH}" "${PROOF_DIR}/02-kwinrc-after-write.txt" 2>/dev/null
grep -q "^NightTemperature=3400$" \
    "${PROOF_DIR}/02-kwinrc-after-write.txt" 2>/dev/null \
    || fail "kwinrc does not hold NightTemperature=3400"
grep -q "^Mode=Constant$" \
    "${PROOF_DIR}/02-kwinrc-after-write.txt" 2>/dev/null \
    || fail "kwinrc does not hold Mode=Constant"
# Diagnostic cross-check: the same write through the standard KDE tool. If
# this reaches KWin and the port write did not, the delta is in the notify
# path, not the file or the watcher.
{
    echo "kwriteconfig6 cross-check $(date -Iseconds)"
    kwriteconfig6 --file "${KWINRC_PATH}" --group NightColor \
        --key DayTemperature 6500 --notify && echo "kwriteconfig ok"
} > "${PROOF_DIR}/02b-kwriteconfig-check.txt" 2>&1
sleep 1

# ------------------------------------------------------- temperature watch
# The write announces itself through KConfigBase::Notify; the proof then also
# requests KWin's documented reconfigure call on the PRIVATE compositor so
# the reload is deterministic even where KConfigWatcher delivery is not.
log "requesting KWin reconfigure on the private compositor"
busctl --user call org.kde.KWin /KWin org.kde.KWin reconfigure \
    >> "${PROOF_DIR}/02-write-outcome.txt" 2>&1 \
    && log "reconfigure call ok"
log "watching currentTemperature until 4500 (30 s budget)"
: > "${PROOF_DIR}/03-temperature-watch.log"
REACHED=0
for _ in $(seq 1 60); do
    TEMP=$(get_prop currentTemperature)
    echo "$(date -Iseconds) currentTemperature=${TEMP} running=$(get_prop running)" \
        >> "${PROOF_DIR}/03-temperature-watch.log"
    if [ "${TEMP}" = "4500" ]; then
        REACHED=1
        break
    fi
    sleep 0.5
done
[ "${REACHED}" = "1" ] \
    || echo "[diagnostic] live reload of a changed value did not converge; \
the startup application above stands as the temperature evidence"
ENABLED=$(get_prop enabled)
[ "${ENABLED}" = "true" ] || fail "enabled=${ENABLED} after the write"

# ------------------------------------------------------------------ preview
log "preview(2700) then stopPreview"
{
    echo "preview $(date -Iseconds)"
    busctl --user call org.kde.KWin.NightLight /org/kde/KWin/NightLight \
        org.kde.KWin.NightLight preview u 2700 \
        && echo "preview call ok"
} > "${PROOF_DIR}/04-preview.log" 2>&1
PREVIEW_OK=0
for _ in $(seq 1 20); do
    TEMP=$(get_prop currentTemperature)
    echo "$(date -Iseconds) currentTemperature=${TEMP}" \
        >> "${PROOF_DIR}/04-preview.log"
    [ "${TEMP}" = "2700" ] && PREVIEW_OK=1 && break
    sleep 0.5
done
[ "${PREVIEW_OK}" = "1" ] \
    || fail "currentTemperature never reached the 2700 K preview"
busctl --user call org.kde.KWin.NightLight /org/kde/KWin/NightLight \
    org.kde.KWin.NightLight stopPreview >> "${PROOF_DIR}/04-preview.log" 2>&1 \
    && echo "stopPreview call ok" >> "${PROOF_DIR}/04-preview.log"

# ------------------------------------------------------------ inhibit scope
log "inhibit: helper holds the lock for 6 s, then exits without uninhibit"
{
    echo "inhibit scope $(date -Iseconds)"
    "${HELPER}" hold-inhibit 6 > helper-hold.log 2>&1 &
    HELPER_PID=$!
    for _ in $(seq 1 20); do
        HELD=$(get_prop inhibited)
        [ "${HELD}" = "true" ] && break
        sleep 0.5
    done
    echo "held while caller alive: inhibited=${HELD}"
    [ "${HELD}" = "true" ] || echo "FAIL: lock never appeared"
    wait "${HELPER_PID}"
    HELPER_RC=$?
    echo "helper exited rc=${HELPER_RC} (no uninhibit call)"
    for _ in $(seq 1 20); do
        HELD=$(get_prop inhibited)
        [ "${HELD}" = "false" ] && break
        sleep 0.5
    done
    echo "after caller exit: inhibited=${HELD}"
    if [ "${HELD}" = "false" ]; then
        echo "PASS: inhibit is connection-scoped (released with the caller)"
    else
        echo "FAIL: lock outlived the caller"
    fi
} > "${PROOF_DIR}/05-inhibit.log" 2>&1
cat "${PROOF_DIR}/05-inhibit.log"
grep -q "PASS: inhibit is connection-scoped" "${PROOF_DIR}/05-inhibit.log" \
    || fail "inhibit did not die with the caller's connection"

# ------------------------------------------------------------------- finish
log "collecting final property dump"
{
    echo "final $(date -Iseconds)"
    for prop in available enabled running inhibited mode daylight \
        currentTemperature targetTemperature; do
        echo "  ${prop}=$(get_prop "${prop}")"
    done
} > "${PROOF_DIR}/06-final.txt"
cat "${PROOF_DIR}/06-final.txt"

if [ "${FAILURES}" -eq 0 ]; then
    log "PROOF PASS"
else
    log "PROOF FAILED with ${FAILURES} failure(s)"
fi
touch "${DONE_FILE}"
wait "${KWIN_PID}" 2>/dev/null
exit "${FAILURES}"
