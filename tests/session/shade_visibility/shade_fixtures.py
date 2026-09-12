# SPDX-License-Identifier: GPL-3.0-or-later
"""Real client fixtures for the shade-visibility nested rows.

Every fixture runs inside the private nested session with the isolated HOME,
XDG, and D-Bus roots the runner prepared. Fixtures that could reach the network
(Firefox, Electron) run inside an unprivileged network namespace that only has
loopback, so a test run cannot open connections off the machine; Firefox is
additionally pinned to a closed local proxy with DNS disabled.
"""

from __future__ import annotations

import functools
import os
import shutil
import signal
import subprocess
import sys
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path

BACKDROP_TITLE = "Shade backdrop"
BACKDROP_COLOUR = "#1f7a4d"
MEMBER_A_TITLE = "Shade member A"
MEMBER_A_COLOUR = "#b03a2e"
MEMBER_B_TITLE = "Shade member B"
MEMBER_B_COLOUR = "#2e5cb0"
MEMBER_C_TITLE = "Shade member C"
MEMBER_C_COLOUR = "#8a2eb0"

ELECTRON_LAUNCHER = "/usr/bin/claude-desktop"
_HERE = Path(__file__).resolve().parent

FIREFOX_OFFLINE_PREFS = """\
user_pref("network.proxy.type", 1);
user_pref("network.proxy.http", "127.0.0.1");
user_pref("network.proxy.http_port", 9);
user_pref("network.proxy.ssl", "127.0.0.1");
user_pref("network.proxy.ssl_port", 9);
user_pref("network.proxy.socks_remote_dns", true);
user_pref("network.dns.disabled", true);
user_pref("network.captive-portal-service.enabled", false);
user_pref("network.connectivity-service.enabled", false);
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.aboutwelcome.enabled", false);
user_pref("browser.startup.homepage_override.mstone", "ignore");
user_pref("browser.safebrowsing.update.enabled", false);
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("datareporting.healthreport.uploadEnabled", false);
user_pref("toolkit.telemetry.enabled", false);
user_pref("app.normandy.enabled", false);
user_pref("extensions.update.enabled", false);
"""

# AGENT-GUARD: the Electron fixture (Claude desktop) refuses to start when a
# Chromium debugging or network-override switch is on its command line; never
# add one. Offline isolation comes from this namespace instead, which keeps the
# caller's uid/gid (Chromium will not run as root) and leaves path-based
# Wayland, D-Bus, and X11 sockets reachable.
OFFLINE_NAMESPACE = ["unshare", "--user", f"--map-user={os.getuid()}",
                     f"--map-group={os.getgid()}", "--net", "--"]
NETWORK_KINDS = frozenset({"firefox-wayland", "firefox-x11", "electron"})

# Kind -> (description, required executable or None).
KINDS: dict[str, tuple[str, str | None]] = {
    "gtk-csd": ("GTK4 native Wayland, client-side decorations", None),
    "gtk-ssd": ("GTK4 native Wayland, server-side QindaQt decoration", None),
    "gtk-borderless": ("GTK4 native Wayland, undecorated", None),
    "gtk-x11-csd": ("GTK4 through XWayland, client-side decorations", None),
    "xterm": ("xterm through XWayland, server-side decoration", "xterm"),
    "weston-terminal": ("weston-terminal native Wayland, client-side decorations",
                        "weston-terminal"),
    "firefox-wayland": ("Firefox native Wayland (offline profile)", "firefox-bin"),
    "firefox-x11": ("Firefox through XWayland (offline profile)", "firefox-bin"),
    "electron": ("Electron (claude-desktop, unmodified, offline namespace)", ELECTRON_LAUNCHER),
}


@functools.cache
def offline_namespace_available() -> bool:
    try:
        completed = subprocess.run([*OFFLINE_NAMESPACE, "true"], capture_output=True,
                                   timeout=10, check=False)
    except (OSError, subprocess.TimeoutExpired):
        return False
    return completed.returncode == 0


def missing_executable(kind: str) -> str | None:
    """The prerequisite a kind needs but the host lacks, or None."""
    if kind not in KINDS:
        raise ValueError(f"unknown fixture kind {kind!r}")
    required = KINDS[kind][1]
    if required is not None and not (shutil.which(required) or Path(required).exists()):
        return required
    if kind in NETWORK_KINDS and not offline_namespace_available():
        return "an unprivileged network namespace (unshare --user --net)"
    return None


@dataclass
class MemberFixture:
    kind: str
    matches: Callable[[str], bool]
    # Hands the fixture a real xdg-activation token, or None when the client
    # has no scriptable activation path.
    request_activation: Callable[[str], None] | None
    # Solid content colour for unroll pixel checks, or None for real apps.
    content_colour: str | None


class FixtureLauncher:
    """Starts and tears down fixture processes for one nested session."""

    def __init__(self, output: Path, event_log: Path, token_file: Path) -> None:
        self._output = output
        self._event_log = event_log
        self._token_file = token_file
        self._children: list[subprocess.Popen] = []

    def spawn(self, arguments: list[str], extra_env: dict[str, str] | None = None) -> subprocess.Popen:
        environment = dict(os.environ)
        environment.update(extra_env or {})
        log = (self._output / f"child-{len(self._children)}.log").open("w", encoding="utf-8")
        process = subprocess.Popen(arguments, env=environment, stdout=log,
                                   stderr=subprocess.STDOUT)
        self._children.append(process)
        return process

    def gtk(self, title: str, colour: str, mode: str, width: int, height: int,
            backend: str = "wayland") -> subprocess.Popen:
        return self.spawn(
            [sys.executable, str(_HERE / "shade_fixture_gtk.py"), title, colour,
             str(self._event_log), mode, str(width), str(height)],
            {"GDK_BACKEND": backend, "GTK_A11Y": "none", "NO_AT_BRIDGE": "1",
             "SHADE_TOKEN_FILE": str(self._token_file)})

    def member_b(self, kind: str) -> MemberFixture:
        if kind.startswith("gtk-"):
            mode = {"gtk-csd": "csd", "gtk-ssd": "ssd", "gtk-borderless": "borderless",
                    "gtk-x11-csd": "csd"}[kind]
            backend = "x11" if kind == "gtk-x11-csd" else "wayland"
            process = self.gtk(MEMBER_B_TITLE, MEMBER_B_COLOUR, mode, 560, 380, backend)
            return MemberFixture(kind, lambda title: title == MEMBER_B_TITLE,
                                 lambda _token: process.send_signal(signal.SIGUSR1),
                                 MEMBER_B_COLOUR)
        if kind in ("firefox-wayland", "firefox-x11"):
            return self._firefox(kind)
        if kind == "electron":
            return self._electron()
        if kind == "xterm":
            self.spawn(["xterm", "-title", "Shade xterm", "-bg", MEMBER_B_COLOUR,
                        "-geometry", "80x24"])
            return MemberFixture(kind, lambda title: title == "Shade xterm", None, None)
        if kind == "weston-terminal":
            self.spawn(["weston-terminal", "--shell=/bin/sh"])
            return MemberFixture(
                kind, lambda title: "Terminal" in title or "weston" in title.lower(), None, None)
        raise ValueError(f"unknown fixture kind {kind!r}")

    def _firefox(self, kind: str) -> MemberFixture:
        profile = self._output / "firefox-profile"
        profile.mkdir(exist_ok=True)
        (profile / "user.js").write_text(FIREFOX_OFFLINE_PREFS, encoding="utf-8")
        wayland = kind == "firefox-wayland"
        environment = {"MOZ_ENABLE_WAYLAND": "1" if wayland else "0",
                       "GDK_BACKEND": "wayland" if wayland else "x11",
                       "MOZ_CRASHREPORTER_DISABLE": "1"}
        arguments = [*OFFLINE_NAMESPACE, "firefox-bin", "--profile", str(profile), "about:blank"]
        self.spawn(arguments, environment)

        def remote_open(token: str) -> None:
            # A second invocation remotes into the running instance, which
            # then activates itself with the handed-over token (a clicked link).
            self.spawn(arguments, {**environment, "XDG_ACTIVATION_TOKEN": token,
                                   "DESKTOP_STARTUP_ID": token})

        return MemberFixture(kind, lambda title: "Firefox" in title or "Mozilla" in title,
                             remote_open, None)

    def _electron(self) -> MemberFixture:
        arguments = [*OFFLINE_NAMESPACE, ELECTRON_LAUNCHER]
        self.spawn(arguments)

        def second_instance(token: str) -> None:
            self.spawn(arguments, {"XDG_ACTIVATION_TOKEN": token, "DESKTOP_STARTUP_ID": token})

        known = {BACKDROP_TITLE, MEMBER_A_TITLE}
        return MemberFixture("electron", lambda title: title not in known and bool(title),
                             second_instance, None)

    def terminate_all(self) -> None:
        for child in self._children:
            if child.poll() is None:
                child.terminate()
        for child in self._children:
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=5)
