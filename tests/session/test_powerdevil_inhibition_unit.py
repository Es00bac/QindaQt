# SPDX-License-Identifier: GPL-3.0-or-later
"""Host-safe contract tests for the PowerDevil qualification runner."""

from __future__ import annotations

import argparse
import stat
import tempfile
import unittest
from pathlib import Path

from test_powerdevil_inhibition_nested import (
    EXPECTED_RELEASE,
    _configure_private_runtime,
    _exact_release_inputs,
)
from desktop_session_sandbox import SandboxContractError


class PowerDevilRunnerContractTests(unittest.TestCase):
    def _executable(self, root: Path, name: str, body: str) -> Path:
        path = root / name
        path.write_text("#!/bin/sh\n" + body, encoding="utf-8")
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def test_private_config_sets_every_profile_without_suspend(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            config = Path(temporary) / "config"
            socket = _configure_private_runtime({
                "XDG_CONFIG_HOME": str(config),
                "QINDAQT_SESSION_RUN_ID": "1" * 32,
            })
            self.assertEqual(socket, "qindaqt-powerdevil-111111111111")
            document = (config / "powerdevilrc").read_text(encoding="utf-8")
            for profile in ("AC", "Battery", "LowBattery"):
                self.assertIn(f"[{profile}][Display]", document)
                self.assertIn(f"[{profile}][SuspendAndShutdown]", document)
            self.assertEqual(document.count("TurnOffDisplayIdleTimeoutSec=30"), 3)
            self.assertEqual(document.count("AutoSuspendAction=0"), 3)
            self.assertEqual(
                (config / "xdg-desktop-portal/portals.conf").read_text(encoding="utf-8"),
                "[preferred]\ndefault=none\norg.freedesktop.impl.portal.Inhibit=kde\n",
            )

    def test_exact_release_accepts_revision_suffix_and_ignores_kwin_x11(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            qlist = self._executable(
                root, "qlist",
                "cat <<'EOF'\n"
                "kde-plasma/kscreenlocker-6.6.6\n"
                "kde-plasma/kwin-6.6.6\n"
                "kde-plasma/kwin-x11-6.6.6\n"
                "kde-plasma/libkscreen-6.6.6-r1\n"
                "kde-plasma/powerdevil-6.6.6-r1\n"
                "kde-plasma/xdg-desktop-portal-kde-6.6.6\n"
                "EOF\n",
            )
            kwin = self._executable(root, "kwin_wayland", "echo 'kwin 6.6.6'\n")
            versions = _exact_release_inputs(
                argparse.Namespace(qlist=qlist, kwin_wayland=kwin)
            )
            self.assertEqual(set(versions.values()), {EXPECTED_RELEASE})
            self.assertNotIn("kde-plasma/kwin-x11", versions)

    def test_exact_release_rejects_one_mismatched_provider(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            qlist = self._executable(
                root, "qlist",
                "for atom in kwin kscreenlocker libkscreen powerdevil xdg-desktop-portal-kde; do "
                "version=6.6.6; [ \"$atom\" = powerdevil ] && version=6.6.5; "
                "echo kde-plasma/$atom-$version; done\n",
            )
            kwin = self._executable(root, "kwin_wayland", "echo 'kwin 6.6.6'\n")
            with self.assertRaisesRegex(SandboxContractError, "not release 6.6.6"):
                _exact_release_inputs(argparse.Namespace(qlist=qlist, kwin_wayland=kwin))


if __name__ == "__main__":
    unittest.main()
