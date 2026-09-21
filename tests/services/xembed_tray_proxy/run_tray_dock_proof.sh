#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# ADR-0229 end-to-end proof. Stands up a private Xwayland display and a
# private session bus, runs the XEmbed tray proxy against both, docks a probe
# icon, and reports whether a StatusNotifierItem appeared.
#
# AGENT-CONTRACT: never touches the operator's X display or the session bus,
# and always tears down what it started. Exits 77 (ctest SKIP_RETURN_CODE)
# when the host cannot provide Xwayland, a Wayland compositor or dbus-daemon,
# so a headless CI box reports a skip rather than a false failure.
set -uo pipefail

proxy=${QINDAQT_TRAY_PROXY:?}
probe=${QINDAQT_TRAY_PROBE:?}
work=${QINDAQT_TRAY_WORK_DIR:?}

command -v Xwayland >/dev/null 2>&1 || { echo "skip: no Xwayland"; exit 77; }
command -v dbus-daemon >/dev/null 2>&1 || { echo "skip: no dbus-daemon"; exit 77; }
[ -n "${WAYLAND_DISPLAY:-}" ] && [ -n "${XDG_RUNTIME_DIR:-}" ] \
  || { echo "skip: no Wayland session for Xwayland to attach to"; exit 77; }

rm -rf "$work"; mkdir -p "$work"

# AGENT-GUARD: the bus socket lives in a SHORT directory, not in the build
# tree. An AF_UNIX path may not exceed 108 bytes, and a lane build root under
# .cache/<wave>/wt/<lane>/.build already blows that on its own — dbus-daemon
# then dies with "Socket name too long" and the row skips for the wrong
# reason. Logs stay in the work directory where ctest can find them.
sockets=$(mktemp -d /tmp/qqtray.XXXXXX) || { echo "skip: no temp dir"; exit 77; }

display=""
for n in $(seq 77 90); do
  if [ ! -e "/tmp/.X11-unix/X$n" ]; then display=":$n"; break; fi
done
[ -n "$display" ] || { echo "skip: no free X display number"; exit 77; }

xwayland_pid=""; proxy_pid=""; bus_pid=""
cleanup() {
  for pid in "$probe_pid" "$proxy_pid" "$xwayland_pid" "$bus_pid"; do
    [ -n "${pid:-}" ] && kill "$pid" 2>/dev/null
  done
  wait 2>/dev/null
  [ -n "${sockets:-}" ] && rm -rf "$sockets"
}
probe_pid=""
trap cleanup EXIT

cat > "$work/bus.conf" <<EOF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=$sockets</listen>
  <policy context="default">
    <!-- AGENT-GUARD: a private session bus needs receive_sender as well as
         send_destination. With send+own alone the daemon logs "Rejected
         receive message, 0 matched rules" and every client fails to complete
         its Hello, so the proxy reports "cannot reach the D-Bus session bus"
         while the socket looks perfectly healthy. -->
    <allow send_destination="*"/>
    <allow receive_sender="*"/>
    <allow own="*"/>
  </policy>
</busconfig>
EOF

# AGENT-GUARD: background it, do not --fork it. A --fork'd dbus-daemon on this
# host stays alive with its socket present but refuses every connection
# ("Transport endpoint is not connected"); backgrounding the daemon and reading
# the address it prints works. Both the proxy and the probe connect to this
# address, never to the session bus.
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

QINDAQT_TRAY_PROBE_DISPLAY="$display" QINDAQT_TRAY_PROBE_BUS="$bus_address" "$probe"
status=$?
echo "--- proxy log ---"
cat "$work/proxy.log" 2>/dev/null
exit "$status"
