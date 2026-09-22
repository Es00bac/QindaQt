#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Regression proof: the tray proxy must terminate when its X display dies, and
# must never spin while it decides to.
#
# The session's X server (XWayland) exits with the session, but the user's
# D-Bus session bus outlives it. The constructing-bus lifetime guard in main()
# therefore never fires for that case. Before this was fixed, the XCB
# descriptor stayed readable at EOF, xcb_poll_for_event() returned nullptr
# immediately, and the still-enabled QSocketNotifier refired on every event
# loop pass: orphaned proxies were observed burning a full CPU core for hours
# after their session ended, slowing every animation on the machine.
#
# AGENT-CONTRACT: never touches the operator's X display or the session bus,
# and always tears down what it started. Exits 77 (ctest SKIP_RETURN_CODE)
# when the host cannot provide Xwayland or dbus-daemon.
set -uo pipefail

proxy=${QINDAQT_TRAY_PROXY:?}
work=${QINDAQT_TRAY_WORK_DIR:?}

command -v Xwayland >/dev/null 2>&1 || { echo "skip: no Xwayland"; exit 77; }
command -v dbus-daemon >/dev/null 2>&1 || { echo "skip: no dbus-daemon"; exit 77; }
[ -n "${WAYLAND_DISPLAY:-}" ] && [ -n "${XDG_RUNTIME_DIR:-}" ] \
  || { echo "skip: no Wayland session for Xwayland to attach to"; exit 77; }

rm -rf "$work"; mkdir -p "$work"

# AGENT-GUARD: see run_tray_dock_proof.sh - the bus socket must live in a short
# path, not in the build tree, or dbus-daemon dies with "Socket name too long".
sockets=$(mktemp -d /tmp/qqtrayloss.XXXXXX) || { echo "skip: no temp dir"; exit 77; }

display=""
for n in $(seq 77 90); do
  if [ ! -e "/tmp/.X11-unix/X$n" ]; then display=":$n"; break; fi
done
[ -n "$display" ] || { echo "skip: no free X display number"; exit 77; }

xwayland_pid=""; proxy_pid=""; bus_pid=""
cleanup() {
  for pid in "$proxy_pid" "$xwayland_pid" "$bus_pid"; do
    [ -n "${pid:-}" ] && kill "$pid" 2>/dev/null
  done
  wait 2>/dev/null
  [ -n "${sockets:-}" ] && rm -rf "$sockets"
}
trap cleanup EXIT

cat > "$work/bus.conf" <<EOF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=$sockets</listen>
  <policy context="default">
    <allow send_destination="*"/>
    <allow receive_sender="*"/>
    <allow own="*"/>
  </policy>
</busconfig>
EOF

dbus-daemon --config-file="$work/bus.conf" --print-address \
  > "$sockets/address" 2>"$work/bus.err" &
bus_pid=$!
for _ in $(seq 1 40); do
  [ -s "$sockets/address" ] && break
  kill -0 "$bus_pid" 2>/dev/null || break
  sleep 0.25
done
bus_address=$(head -1 "$sockets/address" 2>/dev/null)
[ -n "${bus_address:-}" ] || { echo "skip: private bus did not start"; cat "$work/bus.err"; exit 77; }

Xwayland "$display" > "$work/xwayland.log" 2>&1 &
xwayland_pid=$!
for _ in $(seq 1 40); do
  [ -e "/tmp/.X11-unix/X${display#:}" ] && break
  sleep 0.25
done
[ -e "/tmp/.X11-unix/X${display#:}" ] \
  || { echo "skip: Xwayland did not come up on $display"; tail -5 "$work/xwayland.log"; exit 77; }

"$proxy" --display "$display" --bus-address "$bus_address" > "$work/proxy.log" 2>&1 &
proxy_pid=$!
for _ in $(seq 1 40); do
  grep -q "claimed the XEmbed tray selection" "$work/proxy.log" 2>/dev/null && break
  kill -0 "$proxy_pid" 2>/dev/null || { echo "fail: the proxy exited early"; cat "$work/proxy.log"; exit 1; }
  sleep 0.25
done
kill -0 "$proxy_pid" 2>/dev/null || { echo "fail: the proxy never reached a claim"; cat "$work/proxy.log"; exit 1; }

# The session ends: its X server dies while the bus keeps running.
kill -9 "$xwayland_pid" 2>/dev/null
xwayland_pid=""

# Sample CPU while it decides. A spinning proxy accumulates ~100 jiffies a
# second on its own; an exiting one accumulates almost nothing.
before=$(awk '{print $14+$15}' "/proc/$proxy_pid/stat" 2>/dev/null || echo 0)
exited=0
for _ in $(seq 1 40); do
  kill -0 "$proxy_pid" 2>/dev/null || { exited=1; break; }
  sleep 0.25
done

if [ "$exited" -ne 1 ]; then
  after=$(awk '{print $14+$15}' "/proc/$proxy_pid/stat" 2>/dev/null || echo 0)
  echo "fail: the proxy outlived its X display by 10s (burned $((after - before))"\
       "jiffies; a full core is ~1000)"
  echo "--- proxy log ---"; cat "$work/proxy.log" 2>/dev/null
  exit 1
fi

proxy_pid=""
grep -q "the X display is gone" "$work/proxy.log" 2>/dev/null \
  || { echo "fail: the proxy exited without reporting display loss";
       cat "$work/proxy.log"; exit 1; }

echo "pass: the proxy exited when its X display died"
echo "--- proxy log ---"
cat "$work/proxy.log" 2>/dev/null
exit 0
