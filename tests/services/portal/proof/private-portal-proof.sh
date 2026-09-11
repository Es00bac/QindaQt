#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Real-backend private smoke for the ADR-0133 routing decision.
# Runs under `qq-private gap-portals`: private session bus, no live display,
# private XDG dirs. Starts a headless virtual KWin, a private PipeWire stack,
# the real xdg-desktop-portal-kde, and the real xdg-desktop-portal frontend
# with the staged routing table, then records exactly how far a
# non-interactive Screenshot and a ScreenCast session get. A consent dialog
# that needs a human click is a valid stopping point and is recorded as such.
set -uo pipefail

PROOF=${QQ_PROOF_DIR:?qq-private must set QQ_PROOF_DIR}
BUILD=${QQ_BUILD_ROOT:?qq-private must set QQ_BUILD_ROOT}
WORKTREE=${QQ_WORKTREE:?qq-private must set QQ_WORKTREE}
LOGS="$PROOF/logs"
SUMMARY="$PROOF/summary.md"
mkdir -p "$LOGS"

kwin_pid=""; kde_pid=""; frontend_pid=""; pipewire_pid=""; wireplumber_pid=""
monitor_pid=""; store_pid=""
cleanup() {
    for pid in "$monitor_pid" "$frontend_pid" "$kde_pid" "$wireplumber_pid" \
               "$pipewire_pid" "$store_pid"; do
        [ -n "$pid" ] && kill -TERM "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null
    done
    [ -n "$kwin_pid" ] && kill -TERM "$kwin_pid" 2>/dev/null
    wait "$kwin_pid" 2>/dev/null
}
trap cleanup EXIT

record() { printf '%s\n' "$*" >> "$SUMMARY"; }
wait_socket() {
    local path=$1 name=$2 i
    for i in $(seq 1 100); do
        [ -e "$path" ] && return 0
        sleep 0.2
    done
    record "FAIL: $name never created $path"
    return 1
}
wait_name() {
    gdbus wait --session --timeout 20 "$1" >/dev/null 2>&1
}

: > "$SUMMARY"
record "# gap-portals private smoke (real backends)"
record ""
record "- host stack: KWin $(kwin_wayland --version 2>/dev/null | awk '{print $2}')," \
       "xdg-desktop-portal 1.20.4, xdg-desktop-portal-kde 6.6.6"
record "- session bus: private (dbus-run-session via qq-private)"

# Staged portal metadata: every host declaration plus the built QindaQt
# declaration and the real routing file, so the frontend resolves exactly the
# ADR-0133 table. Two layouts, because there are two ways a frontend can
# start: XDG_DESKTOP_PORTAL_DIR expects the declarations flat beside the conf
# files, while a dbus-activated fallback frontend (started with the session
# daemon's environment) reads the standard XDG_DATA_HOME subdirectory layout.
FLAT="$XDG_DATA_HOME/portal-smoke"
STAGE="$XDG_DATA_HOME/xdg-desktop-portal"
mkdir -p "$FLAT" "$STAGE/portals"
cp /usr/share/xdg-desktop-portal/portals/*.portal "$FLAT"/ 2>/dev/null
cp "$WORKTREE/src/services/portal/data/qindaqt.portal" "$FLAT"/
cp "$WORKTREE/src/services/portal/data/qindaqt-portals.conf" "$FLAT"/
cp "$FLAT"/*.portal "$STAGE/portals"/
cp "$WORKTREE/src/services/portal/data/qindaqt-portals.conf" "$STAGE/portals.conf"
cp "$WORKTREE/src/services/portal/data/qindaqt-portals.conf" "$STAGE/qindaqt-portals.conf"
export XDG_DESKTOP_PORTAL_DIR="$FLAT"
record "- staged metadata: $(ls "$FLAT" | tr '\n' ' ')"

# KService cache: the KDE screencast grant resolves its client through
# KApplicationTrader, and the private XDG cache starts empty.
mkdir -p "$XDG_DATA_HOME/applications" "$XDG_CONFIG_HOME/menus.applications-merged"
cat > "$XDG_DATA_HOME/applications/org.qindaqt.gap-portals-smoke.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=QindaQt gap-portals smoke
Exec=true
NoDisplay=true
EOF
if kbuildsycoca6 > "$LOGS/kbuildsycoca6.log" 2>&1; then
    record "- KService cache built (kbuildsycoca6 ok)"
else
    record "- KService cache build FAILED (see logs/kbuildsycoca6.log)"
fi

# Private PipeWire stack for ScreenCast. The frontend's screencast support
# connects at startup and latches a failed connect, so this stack must be up
# before the frontend starts.
pipewire > "$LOGS/pipewire.log" 2>&1 & pipewire_pid=$!
wait_socket "$XDG_RUNTIME_DIR/pipewire-0" "pipewire socket" \
    && record "- private pipewire running (pid $pipewire_pid)"
wireplumber > "$LOGS/wireplumber.log" 2>&1 & wireplumber_pid=$!
record "- private wireplumber started (pid $wireplumber_pid)"

# Headless virtual compositor; the probe keeps it alive until cleanup.
cat > "$XDG_RUNTIME_DIR/qq-gap-portals-probe" <<'EOF'
#!/bin/bash
sleep 1800
EOF
chmod +x "$XDG_RUNTIME_DIR/qq-gap-portals-probe"
kwin_wayland --virtual --socket qq-gap-portals-0 --width 1280 --height 800 \
    --exit-with-session "$XDG_RUNTIME_DIR/qq-gap-portals-probe" \
    > "$LOGS/kwin.log" 2>&1 & kwin_pid=$!
if wait_socket "$XDG_RUNTIME_DIR/qq-gap-portals-0" "virtual KWin socket"; then
    record "- virtual KWin running on qq-gap-portals-0 (pid $kwin_pid)"
else
    record "  kwin log tail: $(tail -3 "$LOGS/kwin.log" | tr '\n' ' ')"
fi

# The real KDE portal backend needs the KDE identity (ADR-0088), the
# compositor socket, and must see the compositor before it starts: it latches
# its screencast probe once.
export XDG_CURRENT_DESKTOP=KDE
export WAYLAND_DISPLAY=qq-gap-portals-0
export QT_QPA_PLATFORM=wayland
/usr/libexec/xdg-desktop-portal-kde > "$LOGS/portal-kde.log" 2>&1 & kde_pid=$!
if wait_name org.freedesktop.impl.portal.desktop.kde; then
    record "- real xdg-desktop-portal-kde owns its backend name (pid $kde_pid)"
else
    record "FAIL: xdg-desktop-portal-kde did not acquire its name"
    record "  portal-kde log tail: $(tail -5 "$LOGS/portal-kde.log" | tr '\n' ' ')"
fi

# The real frontend under the QindaQt identity and the staged routing table.
export XDG_CURRENT_DESKTOP=QindaQt
/usr/libexec/xdg-desktop-portal --verbose > "$LOGS/frontend.log" 2>&1 & frontend_pid=$!
if wait_name org.freedesktop.portal.Desktop; then
    record "- real xdg-desktop-portal frontend owns org.freedesktop.portal.Desktop"
else
    record "FAIL: frontend did not acquire its name"
    record "  frontend log tail: $(tail -5 "$LOGS/frontend.log" | tr '\n' ' ')"
fi

gdbus monitor --session --dest org.freedesktop.portal.Desktop \
    > "$LOGS/portal-signals.log" 2>&1 & monitor_pid=$!
sleep 1

call_portal() {
    gdbus call --session --dest org.freedesktop.portal.Desktop \
        --object-path /org/freedesktop/portal/desktop "$@" 2>&1
}

record "## Screenshot (non-interactive)"
# The permission store is seeded directly so the frontend's grant lookup
# answers "yes" without a human consent click.
/usr/libexec/xdg-permission-store > "$LOGS/permission-store.log" 2>&1 &
store_pid=$!
seed_ok=""
for i in $(seq 1 3); do
    call_portal --object-path /org/freedesktop/impl/portal/PermissionStore \
        --method org.freedesktop.impl.portal.PermissionStore.SetPermission \
        "'screenshot'" "false" "'screenshot'" "''" "['yes']" \
        > "$LOGS/permission-seed.log" 2>&1
    grep -qi "error" "$LOGS/permission-seed.log" || { seed_ok=1; break; }
    sleep 1
done
if [ -n "$seed_ok" ]; then
    record "- permission store seeded (pid $store_pid): granted after $i attempt(s)"
else
    # Without the grant the real KDE backend asks its consent dialog, which
    # no human can click here: the documented valid stopping point.
    record "- permission store seed failed; Screenshot stops at the consent" \
           "dialog (no human present): $(head -c 100 "$LOGS/permission-seed.log" | tr '\n' ' ')"
fi
screenshot_reply=$(call_portal --method org.freedesktop.portal.Screenshot.Screenshot "" "{}")
record "- Screenshot call reply: $screenshot_reply"
sleep 12
record "- frontend access-dialog evidence:" \
       "$(grep -icE 'access dialog|access_dialog' "$LOGS/frontend.log" 2>/dev/null || echo 0) lines"
grep -iE 'access dialog|access_dialog' "$LOGS/frontend.log" 2>/dev/null | head -2 >> "$SUMMARY"
record "- Response signals observed: $(grep -c Response "$LOGS/portal-signals.log" 2>/dev/null || echo 0)"
grep Response "$LOGS/portal-signals.log" 2>/dev/null | head -2 >> "$SUMMARY"

record "## ScreenCast session"
cast_reply=$(call_portal --method org.freedesktop.portal.ScreenCast.CreateSession \
    "{'session_handle_token': <'qqgapsmoke'>}")
record "- CreateSession reply: $cast_reply"
sleep 5
record "- frontend session evidence: $(grep -c 'screen cast session' "$LOGS/frontend.log" 2>/dev/null || echo 0) lines"
grep 'screen cast session' "$LOGS/frontend.log" 2>/dev/null | head -2 >> "$SUMMARY"
record "- screencast lines in frontend log:" "$(grep -icE 'screencast|pipewire' "$LOGS/frontend.log" 2>/dev/null || echo 0)"
grep -iE 'screencast|pipewire' "$LOGS/frontend.log" 2>/dev/null | head -3 >> "$SUMMARY"

record "## Where it stopped"
record "- Response signals seen in total:" \
       "$(grep -c Response "$LOGS/portal-signals.log" 2>/dev/null || echo 0)"
grep Response "$LOGS/portal-signals.log" 2>/dev/null | head -3 >> "$SUMMARY"
record "- full logs in $LOGS (kwin, portal-kde, frontend, pipewire," \
       "wireplumber, portal-signals, smoke driver)"
kill -TERM "$monitor_pid" 2>/dev/null; monitor_pid=""
echo "smoke driver complete; summary in $SUMMARY"
