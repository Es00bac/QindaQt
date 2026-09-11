#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fixture tests for tools/keyring-check/qindaqt-keyring-check parsing.

Runs the check tool against fixture files that stand in for busctl replies,
PAM stacks, and the portal routing conf, and asserts the reported lines and
exit code. AGENT-GUARD: the fixtures must keep covering the dash-prefixed
"-auth" PAM convention; the real sddm stack only carries pam_gnome_keyring.so
with that prefix.
"""

import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
TOOL = HERE.parent.parent.parent / "tools" / "keyring-check" / "qindaqt-keyring-check"
FIXTURES = HERE / "fixtures"

ENV_NAMES = {
    "busctl_list": "QINDAQT_KEYRING_CHECK_BUSCTL_LIST",
    "read_alias": "QINDAQT_KEYRING_CHECK_READ_ALIAS",
    "locked": "QINDAQT_KEYRING_CHECK_COLLECTION_LOCKED",
    "pam_dir": "QINDAQT_KEYRING_CHECK_PAM_DIR",
    "portal_conf": "QINDAQT_KEYRING_CHECK_PORTAL_CONF",
}


def run_case(pam_dir, portal_conf, busctl_list=None, read_alias=None, locked=None,
             extra_remove=()):
    environment = dict(os.environ)
    for name in ENV_NAMES.values():
        environment.pop(name, None)
    for name in extra_remove:
        environment.pop(name, None)
    overrides = {
        ENV_NAMES["busctl_list"]: busctl_list,
        ENV_NAMES["read_alias"]: read_alias,
        ENV_NAMES["locked"]: locked,
        ENV_NAMES["pam_dir"]: str(FIXTURES / pam_dir),
        ENV_NAMES["portal_conf"]: str(FIXTURES / portal_conf),
    }
    for name, value in overrides.items():
        if value is not None:
            environment[name] = str(value)
    return subprocess.run([sys.executable, str(TOOL)], env=environment,
                          capture_output=True, timeout=30, check=False)


def expect(condition, description, result):
    if not condition:
        raise AssertionError(
            f"{description}\nstdout: {result.stdout.decode(errors='replace')}\n"
            f"stderr: {result.stderr.decode(errors='replace')}\n"
            f"exit: {result.returncode}")


def healthy():
    return {
        "busctl_list": FIXTURES / "busctl-list-owned.txt",
        "read_alias": FIXTURES / "readalias-login.txt",
        "locked": FIXTURES / "locked-false.txt",
    }


def main():
    if not TOOL.is_file():
        raise AssertionError(f"check tool missing: {TOOL}")

    result = run_case("pam-present", "portal-routed.conf", **healthy())
    text = result.stdout.decode(errors="replace")
    expect(result.returncode == 0, "healthy fixtures must pass", result)
    for needle in ("provider: gnome-keyring-daemon (pid 12345)",
                   "default-collection: unlocked",
                   "pam-gnome-keyring: present (auth: yes, password: yes, session: yes)",
                   "portal-secret-route: routed (gnome-keyring via fixture)",
                   "summary: pass (0)"):
        expect(needle in text, f"expected {needle!r} in output", result)

    # Properties.Get answers with a variant; the live session printed
    # "v b false" and the check must read it as unlocked, not unknown.
    result = run_case("pam-present", "portal-routed.conf",
                      **{**healthy(), "locked": FIXTURES / "locked-variant-false.txt"})
    text = result.stdout.decode(errors="replace")
    expect(result.returncode == 0, "a variant Locked reply must pass", result)
    expect("default-collection: unlocked" in text,
           "a variant Locked reply must report unlocked", result)

    result = run_case("pam-present", "portal-routed.conf",
                      busctl_list=FIXTURES / "busctl-list-none.txt")
    text = result.stdout.decode(errors="replace")
    expect(result.returncode == 1, "missing provider must be a finding", result)
    expect("provider: none" in text and "default-collection: unknown" in text
           and "summary: findings (1)" in text,
           "missing provider must report none and one finding", result)

    result = run_case("pam-present", "portal-routed.conf",
                      **{**healthy(), "locked": FIXTURES / "locked-true.txt"})
    text = result.stdout.decode(errors="replace")
    expect(result.returncode == 1, "locked collection must be a finding", result)
    expect("default-collection: locked" in text
           and "summary: findings (1)" in text,
           "locked collection must report locked and one finding", result)

    result = run_case("pam-missing", "portal-unrouted.conf", **healthy())
    text = result.stdout.decode(errors="replace")
    expect(result.returncode == 1, "pam and portal gaps must be findings", result)
    expect("pam-gnome-keyring: missing" in text
           and "portal-secret-route: missing" in text
           and "summary: findings (2)" in text,
           "pam and portal gaps must report two findings", result)

    result = run_case("pam-partial", "portal-routed.conf", **healthy())
    text = result.stdout.decode(errors="replace")
    expect(result.returncode == 1, "partial pam stack must be a finding", result)
    expect("pam-gnome-keyring: partial (auth: yes, password: no, session: yes)" in text,
           "partial pam stack must name the missing password line", result)

    environment = dict(os.environ)
    for name in ENV_NAMES.values():
        environment.pop(name, None)
    environment[ENV_NAMES["busctl_list"]] = "/nonexistent/busctl-list.txt"
    result = subprocess.run([sys.executable, str(TOOL)], env=environment,
                            capture_output=True, timeout=30, check=False)
    expect(result.returncode == 2, "unreadable fixture must be an internal error",
           result)
    expect(b"error:" in result.stderr, "internal error must explain itself", result)

    print("KEYRING-CHECK PARSING OK: 6 cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
