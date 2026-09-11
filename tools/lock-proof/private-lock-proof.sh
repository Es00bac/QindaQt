#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Private, repeatable proof that KWin's embedded KScreenLocker locks a
# QindaQt-style virtual session on demand (ADR-0132, deliverable 7).
#
# Run only through the lane helper:
#   qq-private gap-lock-screen tools/lock-proof/private-lock-proof.sh
# The helper runs this script inside dbus-run-session with no live display,
# a private XDG_RUNTIME_DIR, and fresh XDG_*_HOME directories, and hands us
# QQ_PROOF_DIR / QQ_BUILD_ROOT / QQ_WORKTREE. Everything started here is
# killed when the helper ends. The live session is never touched.

set -uo pipefail

: "${QQ_PROOF_DIR:?QQ_PROOF_DIR is required}"
PROOF_DIR="$QQ_PROOF_DIR"
SOCKET_NAME="qq-gap-lock-screen-0"
LOG="$PROOF_DIR/proof.log"
KWIN_LOG="$PROOF_DIR/kwin.log"
LOCK_ERROR="$PROOF_DIR/lock-error.log"
GREET_PROCESSES="$PROOF_DIR/greet-processes.txt"

mkdir -p "$PROOF_DIR"
: >"$LOG"
log() { printf '%s %s\n' "$(date -u +%H:%M:%S)" "$*" | tee -a "$LOG"; }

fail() {
  log "FAIL $*"
  exit 1
}

command -v kwin_wayland >/dev/null 2>&1 || fail "kwin_wayland not found"
command -v busctl >/dev/null 2>&1 || fail "busctl not found"

# Deterministic locker state inside this disposable session: no grace timer
# (so the greeter is not deferred), explicit password requirement. This file
# belongs to the helper-provided throwaway XDG_CONFIG_HOME, never to ~/.config.
if [ -n "${XDG_CONFIG_HOME:-}" ]; then
  mkdir -p "$XDG_CONFIG_HOME"
  printf '[Daemon]\nLockGrace=0\nRequirePassword=true\nAutolock=false\n' \
    >"$XDG_CONFIG_HOME/kscreenlockerrc"
fi

# The probe is executed by kwin_wayland through --exit-with-session: when the
# probe exits, the compositor ends, so the whole proof is self-terminating.
probe="$PROOF_DIR/lock-probe.sh"
cat >"$probe" <<PROBE
#!/usr/bin/env bash
set -uo pipefail
dir="$PROOF_DIR"
socket="$SOCKET_NAME"
log() { printf '%s %s\n' "\$(date -u +%H:%M:%S)" "\$*" | tee -a "\$dir/proof.log"; }

# The session bus is ours (dbus-run-session); wait for the daemon to accept
# connections before asking KWin for the screen saver.
for _ in \$(seq 1 50); do
  busctl --user --no-pager status >/dev/null 2>&1 && break
  sleep 0.2
done

locked=false
greet=false
lock_error=""

# Lock: org.freedesktop.ScreenSaver.Lock is the documented request every
# QindaQt lock button dispatches. Retry while KWin is still coming up.
for _ in \$(seq 1 50); do
  if busctl --user call org.freedesktop.ScreenSaver /ScreenSaver \\
      org.freedesktop.ScreenSaver Lock >/dev/null 2>"\$dir/lock-error.log"; then
    locked=requested
    break
  fi
  lock_error=\$(tail -n 1 "\$dir/lock-error.log" 2>/dev/null)
  sleep 0.2
done
log "lock-request: \${locked:-refused}"
if [ "\$locked" != requested ]; then
  log "STOP KWin refused the lock request; exact error: \${lock_error:-unknown}"
  exit 1
fi

# GetActive must report true once the lock is engaged.
for _ in \$(seq 1 100); do
  value=\$(busctl --user call org.freedesktop.ScreenSaver /ScreenSaver \\
      org.freedesktop.ScreenSaver GetActive 2>/dev/null || true)
  if [ "\$value" = "b true" ]; then locked=true; break; fi
  sleep 0.1
done
log "get-active: \$locked"

# The greeter process is the expected UI evidence inside THIS private
# session. Matching on the Wayland socket keeps the check away from any
# greeter of a host session.
for _ in \$(seq 1 40); do
  found=""
  for environ in /proc/[0-9]*/environ; do
    grep -azq "WAYLAND_DISPLAY=\$socket" "\$environ" 2>/dev/null || continue
    cmdline="\${environ%/environ}/cmdline"
    if grep -aq kscreenlocker_greet "\$cmdline" 2>/dev/null; then
      found="\${environ%/environ}"
      break
    fi
  done
  if [ -n "\$found" ]; then greet=true; break; fi
  sleep 0.5
done
: >"\$dir/greet-processes.txt"
for environ in /proc/[0-9]*/environ; do
  grep -azq "WAYLAND_DISPLAY=\$socket" "\$environ" 2>/dev/null || continue
  cmdline="\${environ%/environ}/cmdline"
  grep -aq kscreenlocker_greet "\$cmdline" 2>/dev/null || continue
  tr '\0' ' ' <"\$cmdline" >>"\$dir/greet-processes.txt"
  printf '\n' >>"\$dir/greet-processes.txt"
done
log "greeter-process: \$greet"
if [ "\$greet" != true ]; then
  # Structural, not incidental: record the exact session-backend limitation.
  backend_line=\$(grep -iE 'Could not load a session backend' "\$dir/kwin.log" \
    | tail -n 1)
  log "greeter-absence-cause: \${backend_line:-kwin reported no session backend line}"
fi

# Best-effort screenshot over ScreenShot2; KWin may refuse on a private bus.
if command -v python3 >/dev/null 2>&1; then
  python3 - "\$dir/screenshot.png" <<'PY' >"\$dir/screenshot-status.txt" 2>&1 || true
import os
import sys

try:
    import gi
    from gi.repository import Gio, GLib
except Exception as error:  # noqa: BLE001
    print(f"ScreenShot2 unavailable: gi bindings missing ({error})")
    raise SystemExit(1)

bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
read_fd, write_fd = os.pipe()
fd_list = Gio.UnixFDList()
index = fd_list.append(write_fd)
os.close(write_fd)
result = bus.call_with_unix_fd_list_sync(
    "org.kde.KWin",
    "/org/kde/KWin/ScreenShot2",
    "org.kde.KWin.ScreenShot2",
    "CaptureActiveScreen",
    GLib.Variant("(a{sv}h)", ({}, index)),
    None,
    Gio.DBusCallFlags.NONE,
    2000,
    fd_list,
    None,
)
data = b""
while True:
    chunk = os.read(read_fd, 1 << 20)
    if not chunk:
        break
    data += chunk
os.close(read_fd)
metadata = result[0]
with open(sys.argv[1], "wb") as target:
    target.write(data)
print(f"saved {len(data)} bytes; metadata {dict(metadata)}" if data else "ScreenShot2 wrote no data")
PY
  log "screenshot: \$(cat "\$dir/screenshot-status.txt" 2>/dev/null | tail -n 1)"
fi

log "unlock-note: unlocking requires the session password and cannot be automated; the proof ends here with the session locked"
if [ "\$locked" = true ]; then
  if [ "\$greet" = true ]; then
    log "PROOF OK: lock engaged and greeter resident in the private session"
  else
    log "PROOF OK (lock proven; greeter not spawnable without a logind session backend - see greeter-absence-cause)"
  fi
  exit 0
fi
log "PROOF FAILED: locked=\$locked greeter=\$greet"
exit 1
PROBE
chmod +x "$probe"

log "starting kwin_wayland --virtual (socket $SOCKET_NAME)"
export QQ_PROOF_DIR
kwin_wayland --virtual --socket "$SOCKET_NAME" \
  --width 1280 --height 800 --exit-with-session "$probe" \
  >"$KWIN_LOG" 2>&1
status=$?
grep -iE 'screensaver|kscreenlocker|lock|greeter' "$KWIN_LOG" \
  | tail -n 40 >>"$LOG" || true
log "kwin exit status: $status"
grep -q 'PROOF OK' "$LOG" || fail "proof did not complete; see $KWIN_LOG and $LOG"
log "evidence in $PROOF_DIR: proof.log, kwin.log, greet-processes.txt, screenshot files (if granted)"
exit 0
