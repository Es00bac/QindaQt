#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Secret Service provider contract scenario (ADR-0135).

CTest rows invoke this script with --case; the script prepares a disposable
XDG tree, then re-execs itself under dbus-run-session so the private bus is
created with an empty activation directory:

- provider        start gnome-keyring-daemon with a test password, assert
                  org.freedesktop.secrets ownership, that the default alias
                  resolves to an unlocked collection, and that a stored secret
                  round-trips; then lock the collection and assert a lookup
                  returns nothing without prompting.
- wrong-password  create the keyring with a correct password, restart the
                  daemon with a wrong password, and assert the default
                  collection stays locked.
- no-daemon       with no provider and no activation file, assert the client
                  path fails closed within a bounded time.

AGENT-GUARD: every D-Bus interaction is bounded by `timeout` and the scenario
never answers a prompt; tests depend on no prompter being reachable. The
private bus must have no activation directory, otherwise the first client call
would auto-start the installed provider instead of exercising this run's
explicit daemon.
"""

import argparse
import os
import pathlib
import shutil
import signal
import subprocess
import sys
import time

SECRET_TOOL_TIMEOUT = 20
OWNERSHIP_TIMEOUT = 20
LOCK_TIMEOUT = 10

SKIP_EXIT = 77
DAEMON = "gnome-keyring-daemon"
PAYLOAD = b"qindaqt-secret-service-contract-payload"
ATTRIBUTE = ("provider", "qindaqt-secret-service-contract")


class ScenarioError(Exception):
    pass


def require_tools():
    missing = [tool for tool in (DAEMON, "secret-tool", "busctl", "dbus-run-session")
               if shutil.which(tool) is None]
    if missing:
        print(f"SKIP: required tools not installed: {', '.join(missing)}", file=sys.stderr)
        return False
    return True


def require_private_bus(environment):
    # AGENT-GUARD: never fall back to a caller's real session bus; a missing
    # address means the row was launched outside dbus-run-session and must fail.
    if not environment.get("DBUS_SESSION_BUS_ADDRESS"):
        raise ScenarioError("DBUS_SESSION_BUS_ADDRESS is not set; launch inside dbus-run-session")
    if environment.get("QINDAQT_SECRET_SERVICE_PRIVATE") != "1":
        raise ScenarioError("private environment marker missing; refusing to run uncontained")


def private_environment(work, case):
    empty_data_dirs = work / "empty-data-dirs" / "dbus-1"
    empty_data_dirs.mkdir(parents=True, exist_ok=True)
    for name in ("home", "config", "data", "cache", "state", "empty-etc"):
        (work / name).mkdir(parents=True, exist_ok=True)
    # AGENT-GUARD: rows run concurrently in one shared ctest invocation and
    # gnome-keyring keeps control sockets under $XDG_RUNTIME_DIR/keyring; each
    # case therefore gets its own subdirectory of the private runtime dir, or
    # its daemons would fight over one control socket. Only the fixed
    # secret-service-<case> subdirectory is ever removed.
    runtime_base = os.environ.get("XDG_RUNTIME_DIR") or str(work / "runtime")
    runtime = pathlib.Path(runtime_base) / f"secret-service-{case}"
    if runtime.exists():
        shutil.rmtree(runtime)
    runtime.mkdir(mode=0o700, parents=True)
    environment = dict(os.environ)
    environment.update({
        "HOME": str(work / "home"),
        "XDG_CONFIG_HOME": str(work / "config"),
        "XDG_DATA_HOME": str(work / "data"),
        "XDG_CACHE_HOME": str(work / "cache"),
        "XDG_STATE_HOME": str(work / "state"),
        "XDG_CONFIG_DIRS": str(work / "empty-etc"),
        # AGENT-GUARD: XDG_DATA_DIRS must point at a real but empty directory,
        # never at an empty string (which dbus resolves back to /usr/share).
        "XDG_DATA_DIRS": str(work / "empty-data-dirs"),
        "XDG_RUNTIME_DIR": str(runtime),
        "QINDAQT_SECRET_SERVICE_PRIVATE": "1",
    })
    environment.pop("GNOME_KEYRING_CONTROL", None)
    return environment


def bounded(arguments, input=None, timeout=SECRET_TOOL_TIMEOUT, check=True):
    result = subprocess.run(arguments, input=input, capture_output=True,
                            timeout=timeout, check=False)
    if check and result.returncode != 0:
        raise ScenarioError(
            f"command failed ({result.returncode}): {' '.join(arguments)}\n"
            f"stdout: {result.stdout!r}\nstderr: {result.stderr!r}")
    return result


def secrets_owned(timeout=OWNERSHIP_TIMEOUT):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = bounded(["busctl", "--user", "list", "--no-legend", "--no-pager"],
                         check=False, timeout=5)
        if any(line.split()[0:1] == ["org.freedesktop.secrets"]
               for line in result.stdout.decode(errors="replace").splitlines()):
            return True
        time.sleep(0.25)
    return False


def wait_name_gone(timeout=OWNERSHIP_TIMEOUT):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = bounded(["busctl", "--user", "list", "--no-legend", "--no-pager"],
                         check=False, timeout=5)
        if not any(line.split()[0:1] == ["org.freedesktop.secrets"]
                   for line in result.stdout.decode(errors="replace").splitlines()):
            return True
        time.sleep(0.25)
    return False


def busctl_call(*arguments, timeout=LOCK_TIMEOUT):
    return bounded(["busctl", "--user", "call", *arguments], timeout=timeout,
                   check=False)


def object_path_reply(result, what):
    if result.returncode != 0:
        raise ScenarioError(f"{what} failed: {result.stderr!r}")
    for line in result.stdout.decode(errors="replace").splitlines():
        line = line.strip()
        if line.startswith('o "') and line.endswith('"'):
            return line[3:-1]
    raise ScenarioError(f"{what} returned no object path: {result.stdout!r}")


def read_alias():
    return object_path_reply(busctl_call(
        "org.freedesktop.secrets", "/org/freedesktop/secrets",
        "org.freedesktop.Secret.Service", "ReadAlias", "s", "default"), "ReadAlias")


def collection_locked(path):
    result = busctl_call(
        "org.freedesktop.secrets", path,
        "org.freedesktop.DBus.Properties", "Get", "ss",
        "org.freedesktop.Secret.Collection", "Locked")
    if result.returncode != 0:
        raise ScenarioError(f"reading Locked failed: {result.stderr!r}")
    text = result.stdout.decode(errors="replace").strip()
    if text == "b false":
        return False
    if text == "b true":
        return True
    raise ScenarioError(f"unexpected Locked reply: {text!r}")


def start_daemon(password_file):
    return subprocess.Popen(
        [DAEMON, "--start", "--foreground", "--components=secrets", "--unlock"],
        stdin=password_file.open("rb"), stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, preexec_fn=os.setsid)


def stop_daemon(handle):
    if handle.poll() is None:
        try:
            os.killpg(handle.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            handle.wait(timeout=10)
        except subprocess.TimeoutExpired:
            os.killpg(handle.pid, signal.SIGKILL)
            handle.wait(timeout=10)
    handle.stdout.close()
    handle.stderr.close()


def round_trip_secret():
    bounded(["secret-tool", "store", "--label=qindaqt-secret-service-contract",
             ATTRIBUTE[0], ATTRIBUTE[1]], input=PAYLOAD)
    lookup = bounded(["secret-tool", "lookup", ATTRIBUTE[0], ATTRIBUTE[1]])
    if lookup.stdout != PAYLOAD:
        raise ScenarioError(
            f"round-trip mismatch: stored {PAYLOAD!r}, read {lookup.stdout!r}")


def lookup_locked_fails_closed():
    try:
        result = bounded(["secret-tool", "lookup", ATTRIBUTE[0], ATTRIBUTE[1]],
                         check=False)
    except subprocess.TimeoutExpired:
        raise ScenarioError("lookup on locked collection hung instead of failing")
    if result.stdout:
        raise ScenarioError(f"locked collection returned a secret: {result.stdout!r}")
    if result.returncode == 0:
        raise ScenarioError("locked collection lookup unexpectedly succeeded")


def lock_collection(path):
    prompt = object_path_reply(busctl_call(
        "org.freedesktop.secrets", path,
        "org.freedesktop.Secret.Collection", "Lock"), "Lock")
    if prompt != "/":
        # AGENT-NOTE: locking never needs user interaction in gnome-keyring;
        # a prompt Run that fails means the contract changed and must fail.
        run = busctl_call("org.freedesktop.secrets", prompt,
                          "org.freedesktop.Secret.Prompt", "Run", "u", "0")
        if run.returncode != 0:
            raise ScenarioError(f"Prompt.Run failed: {run.stderr!r}")


def write_password(work, name, value):
    password = work / name
    password.write_bytes(value + b"\n")
    password.chmod(0o600)
    return password


def run_provider(work):
    handle = start_daemon(write_password(work, "provider-password",
                                         b"correct-horse-battery-staple"))
    try:
        if not secrets_owned():
            raise ScenarioError("org.freedesktop.secrets never became owned")
        alias = read_alias()
        if collection_locked(alias):
            raise ScenarioError(f"default alias {alias} is locked after unlock")
        round_trip_secret()
        lock_collection(alias)
        if not collection_locked(alias):
            raise ScenarioError("collection still reports unlocked after Lock")
        lookup_locked_fails_closed()
        print("PROVIDER OK: owned, default unlocked, round-trip, lock fail-closed")
    finally:
        stop_daemon(handle)


def run_wrong_password(work):
    handle = start_daemon(write_password(work, "correct-password",
                                         b"correct-horse-battery-staple"))
    try:
        if not secrets_owned():
            raise ScenarioError("org.freedesktop.secrets never became owned on create run")
        alias = read_alias()
        if collection_locked(alias):
            raise ScenarioError("keyring created locked; fixture is invalid")
    finally:
        stop_daemon(handle)
    if not wait_name_gone():
        raise ScenarioError("org.freedesktop.secrets still owned after daemon stop")

    handle = start_daemon(write_password(work, "wrong-password",
                                         b"truly-wrong-password"))
    try:
        if not secrets_owned():
            raise ScenarioError("org.freedesktop.secrets never became owned on wrong run")
        alias = read_alias()
        if not collection_locked(alias):
            raise ScenarioError("wrong unlock password left the collection unlocked")
        lookup_locked_fails_closed()
        print("WRONG-PASSWORD OK: collection stays locked and lookups fail closed")
    finally:
        stop_daemon(handle)


def run_no_daemon(work):
    del work
    if secrets_owned(timeout=2):
        raise ScenarioError("org.freedesktop.secrets unexpectedly owned without a daemon")
    started = time.monotonic()
    try:
        result = bounded(["secret-tool", "lookup", ATTRIBUTE[0], ATTRIBUTE[1]],
                         check=False)
    except subprocess.TimeoutExpired:
        raise ScenarioError("client path hung without a provider")
    elapsed = time.monotonic() - started
    if result.returncode == 0:
        raise ScenarioError("client path succeeded without a provider")
    print(f"NO-DAEMON OK: client failed closed in {elapsed:.2f}s")


def run_inner(arguments):
    if not require_tools():
        return SKIP_EXIT
    try:
        require_private_bus(os.environ)
        {"provider": run_provider,
         "wrong-password": run_wrong_password,
         "no-daemon": run_no_daemon}[arguments.case](arguments.work)
    except ScenarioError as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    except subprocess.TimeoutExpired as error:
        print(f"FAIL: unbounded D-Bus interaction: {error}", file=sys.stderr)
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", required=True,
                        choices=["provider", "wrong-password", "no-daemon"])
    parser.add_argument("--work", required=True, type=pathlib.Path,
                        help="temporary working directory for this run")
    parser.add_argument("--inner", action="store_true",
                        help=argparse.SUPPRESS)
    arguments = parser.parse_args()

    if arguments.inner:
        return run_inner(arguments)

    if not require_tools():
        return SKIP_EXIT
    environment = private_environment(arguments.work, arguments.case)
    try:
        result = subprocess.run(
            ["dbus-run-session", "--", sys.executable, os.path.abspath(__file__),
             "--case", arguments.case, "--work", str(arguments.work), "--inner"],
            env=environment, timeout=240, check=False)
        return result.returncode
    except subprocess.TimeoutExpired:
        print("FAIL: dbus-run-session did not finish within the bounded time",
              file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
