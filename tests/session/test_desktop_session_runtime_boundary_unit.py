# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused S3 runtime, selector, and sandbox boundary regressions."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path, PurePosixPath
from unittest.mock import Mock, patch

from desktop_session_interaction_runtime import (
    _run_interaction,
    _secondary_primary_environment,
)
from desktop_session_sandbox import (
    FONTCONFIG_FILE_PATH,
    ReadOnlyMount,
    SandboxSpec,
    build_bwrap_argv,
    create_run_root,
    remove_run_root,
    sandbox_environment,
)
from desktop_session_host_tools import (
    library_search_roots,
    system_mounts,
    weston_module_map,
)


def executable(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("fixture\n", encoding="utf-8")
    path.chmod(0o700)


class RuntimeBoundaryTests(unittest.TestCase):
    def test_secondary_primary_selector_retains_private_plugin_precedence(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            backend = Path(directory) / "host-plugins/kf6/kscreen/KSC_KWayland.so"
            executable(backend)
            environment = {
                "QT_PLUGIN_PATH": "/private/qt6/plugins",
                "WAYLAND_DISPLAY": "qindaqt-child",
            }
            selector = _secondary_primary_environment(environment, backend)
            self.assertEqual(
                selector["QT_PLUGIN_PATH"].split(":"),
                ["/private/qt6/plugins", str(backend.parents[2])],
            )
            self.assertEqual(selector["WAYLAND_DISPLAY"], "qindaqt-child")
            self.assertEqual(environment["QT_PLUGIN_PATH"], "/private/qt6/plugins")

    def test_secondary_primary_selector_rejects_missing_backend(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            backend = Path(directory) / "plugins/kf6/kscreen/KSC_KWayland.so"
            with self.assertRaisesRegex(RuntimeError, "backend is unavailable"):
                _secondary_primary_environment(
                    {"QT_PLUGIN_PATH": "/private/qt6/plugins"}, backend
                )

    def test_interaction_failure_archives_post_injection_inventories(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            interaction_log = root / "interaction.log"
            failure_log = root / "interaction-failure.log"
            interaction = Mock(pid=80, returncode=8)
            interaction.communicate.return_value = ("", None)
            snapshot = Mock(pid=81, returncode=0)
            snapshot.communicate.return_value = (
                'QINDAQT_DESKTOP_SESSION_PROBE={"outputs":{"status":"ok"},'
                '"developmentShellSurfaces":{"status":"ok","surfaces":[]},'
                '"shellVisibility":{"status":"ok"},"services":['
                '{"name":"org.freedesktop.Notifications","pid":"49",'
                '"status":"owned"}]}\n',
                None,
            )
            state = Mock()
            with patch(
                "desktop_session_interaction_runtime.subprocess.Popen",
                side_effect=[interaction, snapshot],
            ):
                with self.assertRaisesRegex(
                    RuntimeError, "did not return exact evidence"
                ):
                    _run_interaction(
                        Namespace(probe=Path("/opt/qindaqt-tools/probe")),
                        {"WAYLAND_DISPLAY": "qindaqt-child"},
                        state,
                        secondary_output=True,
                        interaction_log_path=interaction_log,
                        failure_log_path=failure_log,
                    )
            captured = failure_log.read_text(encoding="utf-8")
            self.assertIn('"interactionArgument":"--open-notification-center-secondary"', captured)
            self.assertIn('"requestedOutputName":"WL-1"', captured)
            self.assertIn('"interactionReturnCode":8', captured)
            self.assertIn('"developmentShellSurfaces"', captured)
            self.assertIn('"shellVisibility"', captured)
            self.assertIn('"org.freedesktop.Notifications"', captured)
            self.assertEqual(state.track.call_count, 2)

    def test_private_tool_prefix_exports_real_loader_roots(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "runtime/usr"
            tool = prefix / "bin/kwin_wayland"
            executable(tool)
            lib = prefix / "lib"
            weston = lib / "weston"
            plugins = lib / "qt6/plugins"
            qml = lib / "qt6/qml"
            for path in (weston, plugins, qml):
                path.mkdir(parents=True, exist_ok=True)
            (weston / "libexec_weston.so.0").write_text("fixture\n")
            libraries, plugin_roots, qml_roots = library_search_roots(
                [Path(sys.executable), tool]
            )
            self.assertEqual(libraries, [str(lib), str(weston)])
            self.assertEqual(plugin_roots, [str(plugins)])
            self.assertEqual(qml_roots, [str(qml)])
            environment = sandbox_environment(
                run_id="a" * 32,
                uid=os.getuid(),
                stage_bin="/opt/qindaqt/bin",
                system_path=["/usr/bin", str(prefix / "bin")],
                library_path=libraries,
                qt_plugin_path=plugin_roots,
                qml_import_path=qml_roots,
            )
            self.assertEqual(environment["LD_LIBRARY_PATH"].split(":"), libraries)

    def test_generated_mounts_admit_only_the_host_loader_cache_file(self) -> None:
        mounts = system_mounts([Path(sys.executable)])
        cache = Path("/etc/ld.so.cache")
        destination = PurePosixPath("/etc/ld.so.cache")
        cache_mounts = [item for item in mounts if item.destination == destination]
        if cache.is_file():
            self.assertEqual(cache_mounts, [ReadOnlyMount(cache, destination)])
        else:
            self.assertEqual(cache_mounts, [])

    def test_weston_module_map_names_only_the_private_backend_and_shell(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "runtime/usr"
            weston = prefix / "bin/weston"
            backend = prefix / "lib/libweston-15/headless-backend.so"
            shell = prefix / "lib/weston/kiosk-shell.so"
            for path in (weston, backend, shell):
                executable(path)
            self.assertEqual(
                weston_module_map(weston),
                f"headless-backend.so={backend};kiosk-shell.so={shell}",
            )

    def test_bwrap_has_structural_isolation_and_narrow_mounts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            build.mkdir()
            paths = create_run_root(build, "b" * 32)
            bwrap = root / "bwrap"
            executable(bwrap)
            stage, source, system = root / "stage", root / "source", root / "system"
            for item in (stage, source, system):
                item.mkdir()
            probe = root / "probe"
            executable(probe)
            spec = SandboxSpec(
                bwrap=bwrap,
                run_id="b" * 32,
                uid=1000,
                paths=paths,
                stage=ReadOnlyMount(stage, PurePosixPath("/opt/qindaqt")),
                tests=ReadOnlyMount(source, PurePosixPath("/opt/qindaqt-source")),
                probe=ReadOnlyMount(probe, PurePosixPath("/opt/qindaqt-tools/probe")),
                system_mounts=(
                    ReadOnlyMount(system, PurePosixPath("/usr")),
                    ReadOnlyMount(
                        Path("/etc/ld.so.cache"), PurePosixPath("/etc/ld.so.cache")
                    ),
                ),
                environment=sandbox_environment(
                    run_id="b" * 32, uid=1000,
                    stage_bin="/opt/qindaqt/bin", system_path=["/usr/bin"],
                ),
                command=("/usr/bin/python3", "--version"),
            )
            argv = build_bwrap_argv(spec)
            for required in (
                "--unshare-pid", "--unshare-net", "--unshare-ipc",
                "--die-with-parent", "--new-session", "--clearenv",
            ):
                self.assertIn(required, argv)
            joined = " ".join(argv)
            for forbidden in (
                "/dev/input", "/dev/uinput", "--bind / /", "--ro-bind / /",
                "--ro-bind /etc /etc", "--ro-bind /lib /lib",
                "--ro-bind /lib64 /lib64",
            ):
                self.assertNotIn(forbidden, joined)
            etc_mounts = [
                (argv[index + 1], argv[index + 2])
                for index, value in enumerate(argv)
                if value == "--ro-bind" and argv[index + 1].startswith("/etc")
            ]
            self.assertEqual(etc_mounts, [("/etc/ld.so.cache", "/etc/ld.so.cache")])
            aliases = [
                (argv[index + 1], argv[index + 2])
                for index, value in enumerate(argv) if value == "--symlink"
            ]
            self.assertEqual(aliases, [("usr/lib", "/lib"), ("usr/lib64", "/lib64")])
            self.assertLess(argv.index("/lib64"), argv.index("--proc"))
            remove_run_root(paths, build, "b" * 32)

    def test_fontconfig_env_names_the_source_root_fixture(self) -> None:
        environment = sandbox_environment(
            run_id="c" * 32, uid=1000,
            stage_bin="/opt/qindaqt/bin", system_path=["/usr/bin"],
        )
        self.assertEqual(environment["FONTCONFIG_FILE"], FONTCONFIG_FILE_PATH)
        self.assertEqual(
            FONTCONFIG_FILE_PATH,
            "/opt/qindaqt-source/tests/session/fontconfig/private-runtime/fonts.conf",
        )
        # The `tests` ReadOnlyMount binds the whole source_root at
        # /opt/qindaqt-source (see test_desktop_session_nested.py), so this
        # relies on no mount beyond the one every caller already has.
        fixture = Path(__file__).resolve().parent / "fontconfig/private-runtime/fonts.conf"
        self.assertTrue(fixture.is_file())

    def test_fontconfig_fixture_resolves_the_vendored_family_via_fc_match(self) -> None:
        fc_match = shutil.which("fc-match")
        if fc_match is None:
            self.skipTest("fc-match is required to verify fontconfig parsing")
        repo_root = Path(__file__).resolve().parents[2]
        fixture = repo_root / "tests/session/fontconfig/private-runtime/fonts.conf"
        fonts_dir = repo_root / "tests/controls/fonts"
        self.assertTrue(fixture.is_file())
        self.assertTrue((fonts_dir / "NotoSans-Regular.ttf").is_file())
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory) / "cache"
            cache.mkdir()
            # Real fc-match parse of the checked-in fixture: swap only the
            # in-sandbox source path for the real repo path and point the
            # cache at a throwaway directory. Font input stays read-only.
            rewritten = fixture.read_text(encoding="utf-8").replace(
                "/opt/qindaqt-source/tests/controls/fonts", str(fonts_dir)
            ).replace(
                '<dir prefix="xdg">fonts</dir>', ""
            ).replace(
                '<cachedir prefix="xdg">fontconfig</cachedir>',
                f"<cachedir>{cache}</cachedir>",
            )
            private_conf = Path(directory) / "fonts.conf"
            private_conf.write_text(rewritten, encoding="utf-8")
            result = subprocess.run(
                [fc_match, "--format=%{family}\n", "QindaQt Sans"],
                env={**os.environ, "FONTCONFIG_FILE": str(private_conf), "HOME": directory},
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("QindaQt Sans", result.stdout)

    def test_host_python_interpreter_resolves_through_merged_usr_aliases(self) -> None:
        bwrap = shutil.which("bwrap")
        self.assertIsNotNone(bwrap, "bubblewrap is required by the desktop harness")
        python = Path(sys.executable).resolve(strict=True)
        self.assertEqual(python.parts[:2], ("/", "usr"))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build = root / "build"
            build.mkdir()
            paths = create_run_root(build, "c" * 32)
            stage, source = root / "stage", root / "source"
            stage.mkdir()
            source.mkdir()
            probe = root / "probe"
            executable(probe)
            spec = SandboxSpec(
                bwrap=Path(bwrap),
                run_id="c" * 32,
                uid=os.getuid(),
                paths=paths,
                stage=ReadOnlyMount(stage, PurePosixPath("/opt/qindaqt")),
                tests=ReadOnlyMount(source, PurePosixPath("/opt/qindaqt-source")),
                probe=ReadOnlyMount(probe, PurePosixPath("/opt/qindaqt-tools/probe")),
                system_mounts=(ReadOnlyMount(Path("/usr"), PurePosixPath("/usr")),),
                environment=sandbox_environment(
                    run_id="c" * 32, uid=os.getuid(),
                    stage_bin="/opt/qindaqt/bin", system_path=["/usr/bin"],
                ),
                command=(str(python), "--version"),
            )
            argv = build_bwrap_argv(spec)
            joined = " ".join(argv)
            self.assertNotIn("--ro-bind /lib ", joined)
            self.assertNotIn("--ro-bind /lib64 ", joined)
            completed = subprocess.run(
                argv, text=True, capture_output=True, check=False, timeout=10
            )
            self.assertEqual(
                completed.returncode, 0,
                msg=f"{completed.stdout}{completed.stderr}",
            )
            self.assertIn("Python", f"{completed.stdout}{completed.stderr}")
            remove_run_root(paths, build, "c" * 32)


if __name__ == "__main__":
    unittest.main()
