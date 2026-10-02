#!/usr/bin/env python3
"""Actual private video graph for decoder targeting; no native capture authority."""
import argparse
import hashlib
import json
import os
import pathlib
import re
import resource
import select
import signal
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("fixture", type=pathlib.Path)
    parser.add_argument("--expect-fallback-negative", action="store_true")
    parser.add_argument("--evidence-root", type=pathlib.Path, default=pathlib.Path.cwd() / "decoder-graph-evidence")
    args = parser.parse_args()
    args.evidence_root.mkdir(parents=True, exist_ok=True)
    root = pathlib.Path(tempfile.mkdtemp(prefix="private-decoder-", dir=args.evidence_root))
    os.umask(0o077)
    for name in ("runtime", "home", "config", "data", "cache"):
        (root / name).mkdir(mode=0o700)
    env = {"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8", "HOME": str(root / "home"),
           "XDG_RUNTIME_DIR": str(root / "runtime"), "XDG_CONFIG_HOME": str(root / "config"),
           "XDG_DATA_HOME": str(root / "data"), "XDG_CACHE_HOME": str(root / "cache"),
           "DBUS_SYSTEM_BUS_ADDRESS": "unix:path=" + str(root / "no-system-bus"),
           "PIPEWIRE_RUNTIME_DIR": str(root / "runtime"), "PIPEWIRE_REMOTE": "pipewire-decoder-private",
           "QT_FATAL_WARNINGS": "1"}
    config = root / "pipewire.conf"
    config.write_text("""context.properties = { core.daemon = true core.name = pipewire-decoder-private support.dbus = false }
context.spa-libs = { support.* = support/libspa-support videotestsrc = videotestsrc/libspa-videotestsrc }
context.modules = [
 { name = libpipewire-module-protocol-native }
 { name = libpipewire-module-metadata }
 { name = libpipewire-module-spa-node-factory }
 { name = libpipewire-module-client-node }
 { name = libpipewire-module-access }
 { name = libpipewire-module-link-factory }
]
context.objects = [ { factory = spa-node-factory args = { factory.name = support.node.driver node.name = Private-Decoder-Driver priority.driver = 20000 } } ]
""")
    (root / "client.conf").write_bytes(pathlib.Path("/usr/share/pipewire/client.conf").read_bytes())
    wpdir = root / "wireplumber"
    wpdir.mkdir()
    wp_source = pathlib.Path("/usr/share/wireplumber/wireplumber.conf")
    wp = wp_source.read_text()
    wp = wp.replace("policy = {", "policy = {\n    support.logind = disabled\n    support.portal-permissionstore = disabled\n    script.client.access-portal = disabled", 1)
    (wpdir / "wireplumber.conf").write_text(wp)
    env["WIREPLUMBER_CONFIG_DIR"] = str(wpdir)
    env["PIPEWIRE_CONFIG_DIR"] = str(root)
    children, logs = [], []
    result = {"fixture": str(args.fixture.resolve()), "fixture_sha256": hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
              "wireplumber_config_sha256": hashlib.sha256(wp_source.read_bytes()).hexdigest(), "expected_fallback_negative": args.expect_fallback_negative}
    code = 1
    try:
        bus_config = root / "bus.conf"
        bus_config.write_text("<busconfig><type>session</type><listen>unix:path=" + str(root / "bus") + "</listen><auth>EXTERNAL</auth><policy context='default'><allow own='*'/><allow send_destination='*'/><allow receive_sender='*'/></policy></busconfig>")
        bus_log = (root / "dbus.log").open("wb"); logs.append(bus_log)
        bus = subprocess.Popen(["dbus-daemon", "--nofork", "--config-file=" + str(bus_config), "--print-address=1"], env=env, stdout=subprocess.PIPE, stderr=bus_log, start_new_session=True)
        children.append(bus)
        if not select.select([bus.stdout], [], [], 5)[0]:
            raise RuntimeError("private bus address unavailable")
        env["DBUS_SESSION_BUS_ADDRESS"] = bus.stdout.readline().decode().strip()
        for name, command in (("pipewire", ["pipewire", "-c", str(config)]), ("wireplumber", ["wireplumber", "-p", "policy"])):
            log = (root / (name + ".log")).open("wb"); logs.append(log)
            process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            children.append(process)
            if name == "pipewire":
                deadline = time.monotonic() + 5
                while not (root / "runtime/pipewire-decoder-private").exists() and process.poll() is None and time.monotonic() < deadline:
                    time.sleep(.01)
                if not (root / "runtime/pipewire-decoder-private").exists():
                    raise RuntimeError("private PipeWire socket unavailable")
        command = [str(args.fixture.resolve())]
        if args.expect_fallback_negative:
            command.append("retiredOfferedTargetNeverUsesDefault")
        (root / "command.json").write_text(json.dumps(command, indent=2))
        with (root / "fixture.log").open("wb") as log:
            fixture = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            children.append(fixture)
            result["fixture_exit"] = fixture.wait(timeout=45)
        output = (root / "fixture.log").read_text()
        print(output, end="")
        assert all(child.poll() is None for child in children if child is not fixture), "private graph dependency exited"
        if args.expect_fallback_negative:
            actual = re.search(r"actual retired-target result frames=(\d+) error= linked-source=(\d+) unrelated=(\d+)", output)
            assert result["fixture_exit"] != 0 and actual and int(actual[1]) > 0 and actual[2] == actual[3], "old decoder did not demonstrate actual unrelated fallback frames"
            result["actual_fallback_frames"] = int(actual[1])
            result["actual_fallback_node"] = int(actual[2])
        else:
            assert result["fixture_exit"] == 0, "decoder targeting fixture failed"
        code = 0
    finally:
        cleanup = []
        for child in reversed(children):
            if child.poll() is None:
                os.killpg(child.pid, signal.SIGTERM)
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait(timeout=5)
                    cleanup.append({"pid": child.pid, "forced_kill": True})
        for log in logs:
            log.close()
        result.update(exit=code, children=[{"pid": child.pid, "returncode": child.returncode} for child in children], cleanup_errors=cleanup,
                      scoped_cores=[str(path) for path in root.rglob("core*") if path.is_file()])
        (root / "audit.json").write_text(json.dumps(result, indent=2))
        print("DECODER_GRAPH_AUDIT " + json.dumps({"evidence": str(root), **result}))
        if cleanup or result["scoped_cores"]:
            code = 1
    return code


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    raise SystemExit(main())
