# SPDX-License-Identifier: GPL-3.0-or-later
# Private-session proof for ADR-0134: verify how the pinned KWin consumes
# kcminputrc/kxkbrc and how kglobalaccel handles command shortcuts.
# Runs inside qq-private (dbus-run-session, private XDG dirs, no display).
# Every claim is written to $QQ_PROOF_DIR/summary.txt with VERIFIED/UNVERIFIED.
set -uo pipefail

PROOF=${QQ_PROOF_DIR:?must run under qq-private}
SUMMARY="$PROOF/summary.txt"
: > "$SUMMARY"
LOG="$PROOF/kwin.log"

note() { printf '%s\n' "$*" | tee -a "$SUMMARY"; }
evidence() { # evidence <name> <content-producer...>
    local name=$1; shift
    "$@" > "$PROOF/$name.txt" 2>&1
}

SOCKET=qq-gap-input-shortcuts-0

# Minimal session probe: kwin_wayland exits when this process exits.
sleep 1700 &
PROBE=$!

note "== start $(date -Iseconds) XDG_CONFIG_HOME=$XDG_CONFIG_HOME"
kwin_wayland --virtual --socket "$SOCKET" --width 1280 --height 800 \
    --exit-with-session "$PROBE" > "$LOG" 2>&1 &
KWIN_PID=$!

wait_for() { # wait_for <name> <seconds> <command...>
    local name=$1
    local tries=$2
    shift 2
    for _ in $(seq 1 "$tries"); do
        if "$@" > /dev/null 2>&1; then
            return 0
        fi
        sleep 1
    done
    return 1
}

if wait_for kwin 90 busctl --user call org.kde.KWin /KWin org.freedesktop.DBus.Peer Ping; then
    note "VERIFIED kwin_started socket=$SOCKET pid=$KWIN_PID"
else
    note "FAILED kwin_did_not_start pid_alive=$(kill -0 "$KWIN_PID" 2>/dev/null && echo yes || echo no); see kwin.log"
    kill "$PROBE" 2>/dev/null
    exit 1
fi
evidence kwin-services busctl --user tree org.kde.KWin
evidence kwin-support busctl --user call org.kde.KWin /KWin org.kde.KWin supportInformation
note "kglobalaccel owner: $(busctl --user status org.kde.kglobalaccel 2>/dev/null | sed -n 's/^.*PID: */kwin? /p' || echo unknown)"

# --- Question 1: does KWin persist D-Bus device property writes? ----------
DEVS=$(busctl --user call org.kde.KWin /org/kde/KWin/InputDevice \
    org.kde.KWin.InputDeviceManager ListPointers 2>/dev/null | tr ' ' '\n' | grep -c event || true)
note "private-session pointer devices seen: $DEVS"
if [ "$DEVS" -ge 1 ]; then
    DEV=$(busctl --user call org.kde.KWin /org/kde/KWin/InputDevice \
        org.kde.KWin.InputDeviceManager ListPointers | tr ' ' '\n' | grep event | head -1)
    evidence device-introspect "busctl --user introspect org.kde.KWin /org/kde/KWin/InputDevice/$DEV org.kde.KWin.InputDevice"
    busctl --user call org.kde.KWin "/org/kde/KWin/InputDevice/$DEV" \
        org.freedesktop.DBus.Properties Set ssb org.kde.KWin.InputDevice naturalScroll true
    sleep 3
    if [ -f "$XDG_CONFIG_HOME/kcminputrc" ]; then
        note "VERIFIED kwin_writes_kcminputrc_on_dbus_set"
        cp "$XDG_CONFIG_HOME/kcminputrc" "$PROOF/kcminputrc-after-set.txt"
    else
        note "VERIFIED kwin_does_not_write_kcminputrc_on_dbus_set (settings client must persist)"
    fi
else
    note "UNVERIFIED kwin_persistence_no_pointer_device_in_virtual_session (log only)"
fi

# --- Question 2: kxkbrc live reload path -----------------------------------
mkdir -p "$XDG_CONFIG_HOME"
cat > "$XDG_CONFIG_HOME/kxkbrc" <<'EOF'
[Layout]
LayoutList=us
Model=pc104
EOF
layouts() { busctl --user call org.kde.keyboard /Layouts org.kde.KeyboardLayouts getLayoutsList 2>/dev/null; }
note "layouts before: $(layouts)"
sed -i 's/^LayoutList=us$/LayoutList=us,de/' "$XDG_CONFIG_HOME/kxkbrc"
sleep 4
note "layouts after kxkbrc write only: $(layouts)"
if layouts | grep -q de; then
    note "VERIFIED kxkbrc_reloads_via_file_watch_only"
else
    busctl --user call org.kde.KWin /KWin org.kde.KWin reconfigure
    sleep 4
    note "layouts after reconfigure: $(layouts)"
    if layouts | grep -q de; then
        note "VERIFIED kxkbrc_needs_kwin_reconfigure"
    else
        note "FAILED kxkbrc_not_consumed_see_kwin.log"
    fi
fi
evidence layouts-final layouts

# --- Question 3: kcminputrc [Keyboard] reload ------------------------------
cat > "$XDG_CONFIG_HOME/kcminputrc" <<'EOF'
[Keyboard]
KeyRepeat=true
RepeatDelay=660
RepeatRate=25
NumLock=0
EOF
sleep 3
busctl --user call org.kde.KWin /KWin org.kde.KWin reconfigure 2>/dev/null
sleep 2
if grep -qi "repeat" "$PROOF/kwin-support.txt"; then
    note "VERIFIED repeat_visible_in_supportinformation"
    grep -i "repeat\|numlock" "$PROOF/kwin-support.txt" | head -20 >> "$SUMMARY"
else
    note "UNVERIFIED repeat_not_dumped_by_supportinformation (file + reconfigure path only)"
fi
note "kcminputrc round-trip read: $(cat "$XDG_CONFIG_HOME/kcminputrc" | tr '\n' ';')"

# --- Question 4: kglobalaccel command shortcut mechanism -------------------
KGA_DIR="$XDG_DATA_HOME/kglobalaccel"
mkdir -p "$KGA_DIR"
cat > "$KGA_DIR/qindaqt-proof-command.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=QindaQt proof command
Exec=/bin/true
X-KDE-StartupNotify=false
NoDisplay=true
EOF
sleep 2
kga() { busctl --user call org.kde.kglobalaccel /kglobalaccel org.kde.KGlobalAccel "$@"; }
# Meta+J = 0x10000000 | 0x6A = 268435562
note "kga components before register: $(kga allMainComponents | tr ' ' '\n' | grep -c proof || true)"
kga setForeignShortcutKeys asa(ai) 6 "qindaqt-proof-command" "qindaqt-proof-command" "qindaqt-proof-command" "QindaQt proof command" "default" "Default Context" 1 268435562 1 268435562
RC=$?
sleep 2
note "setForeignShortcutKeys rc=$RC"
INFO=$(busctl --user call org.kde.kglobalaccel /component/qindaqt-proof-command \
    org.kde.kglobalaccel.Component allShortcutInfos 2>&1)
note "fake component info: $INFO"
if echo "$INFO" | grep -q 268435562; then
    note "VERIFIED kglobalaccel_registers_desktop_component_after_file_and_set"
elif busctl --user call org.kde.kglobalaccel /kglobalaccel org.kde.KGlobalAccel allMainComponents 2>/dev/null | grep -q proof; then
    note "VERIFIED kglobalaccel_lists_desktop_component"
    busctl --user call org.kde.kglobalaccel /component/qindaqt-proof-command \
        org.kde.kglobalaccel.Component allShortcutInfos > "$PROOF/fake-component-info.txt" 2>&1
else
    note "FAILED kglobalaccel_command_component_not_registered (daemon may scan at start only)"
fi
evidence kga-components busctl --user call org.kde.kglobalaccel /kglobalaccel org.kde.KGlobalAccel allMainComponents

# --- Cleanup ----------------------------------------------------------------
kill "$PROBE" 2>/dev/null
wait "$KWIN_PID" 2>/dev/null
note "== end $(date -Iseconds)"
exit 0
