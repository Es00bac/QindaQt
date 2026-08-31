# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import os
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from argparse import Namespace
from contextlib import nullcontext
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path, PurePosixPath
from unittest.mock import Mock, patch

from desktop_session_measure import (
    MeasurementContractError,
    ProcessSample,
    aggregate_pss_kib,
    cpu_percent,
    parse_proc_stat,
    parse_smaps_rollup,
)
from desktop_session_sandbox import (
    FORBIDDEN_ENVIRONMENT,
    PrivateLaneLock,
    ReadOnlyMount,
    SandboxContractError,
    SandboxSpec,
    build_bwrap_argv,
    create_run_root,
    remove_run_root,
    sandbox_environment,
)
from desktop_session_stage import (
    PRODUCTION_EXECUTABLES,
    StageContractError,
    reset_stage_root,
    resolve_stage,
)
from desktop_session_runtime import _run_interaction, _secondary_primary_environment
from test_desktop_session_nested import (
    LANE_ENVIRONMENT,
    LANE_VALUE,
    AttemptResult,
    _library_search_roots,
    _system_mounts,
    _weston_module_map,
    archive_attempt,
    create_result_root,
    run_outer,
)


HERE = Path(__file__).resolve().parent


def executable(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("fixture\n", encoding="utf-8")
    path.chmod(0o700)


def fake_stage(root: Path) -> None:
    for basename in PRODUCTION_EXECUTABLES.values():
        executable(root / "bin" / basename)
    plugin = root / "lib/qindaqt/plugins/kwin/plugins/qindaqt_compositor.so"
    plugin.parent.mkdir(parents=True)
    plugin.write_text("plugin\n", encoding="utf-8")
    decoration = root / "lib/qindaqt/plugins/org.kde.kdecoration3/org.qindaqt.so"
    decoration.parent.mkdir(parents=True)
    decoration.write_text("decoration\n", encoding="utf-8")
    services = root / "share/dbus-1/services"
    services.mkdir(parents=True)
    (services / "org.qindaqt.Settings1.service").write_text(
        "[D-BUS Service]\nName=org.qindaqt.Settings1\nExec=/usr/bin/qindaqt-settings-service\n",
        encoding="utf-8",
    )
    (services / "org.qindaqt.Audio1.service").write_text(
        "[D-BUS Service]\nName=org.qindaqt.Audio1\nExec=/usr/bin/qindaqt-audio-service\n",
        encoding="utf-8",
    )


def outer_arguments(root: Path, run_id: str) -> Namespace:
    build = root / "build"
    source = root / "source"
    build.mkdir()
    source.mkdir()
    bwrap = root / "bwrap"
    executable(bwrap)
    return Namespace(
        build_root=build,
        source_root=source,
        bwrap=bwrap,
        run_id=run_id,
        print_command_json=False,
    )


class MeasurementTests(unittest.TestCase):
    def test_proc_fixtures_parse(self) -> None:
        fixture = HERE / "fixtures/desktop_session"
        self.assertEqual(parse_smaps_rollup((fixture / "smaps_rollup.txt").read_text()), 3072)
        self.assertEqual(parse_proc_stat((fixture / "proc_stat.txt").read_text()), (120, 30))

    def test_malformed_proc_data_fails(self) -> None:
        for contents in ("", "Pss: 2 MB", "Pss: 1 kB\nPss: 2 kB"):
            with self.subTest(contents=contents):
                with self.assertRaises(MeasurementContractError):
                    parse_smaps_rollup(contents)
        with self.assertRaises(MeasurementContractError):
            parse_proc_stat("12 no delimiters")

    def test_aggregate_and_cpu_are_pid_bound(self) -> None:
        first = ProcessSample(2, 100, 10, 5)
        second = ProcessSample(3, 200, 20, 10)
        self.assertEqual(aggregate_pss_kib([first, second]), 300)
        after = {
            2: ProcessSample(2, 110, 20, 5),
            3: ProcessSample(3, 210, 25, 15),
        }
        self.assertEqual(
            cpu_percent({2: first, 3: second}, after, elapsed_seconds=2, clock_ticks_per_second=100),
            10.0,
        )
        with self.assertRaises(MeasurementContractError):
            aggregate_pss_kib([first, first])


class StageTests(unittest.TestCase):
    def test_direct_stage_resolution_ignores_absolute_descriptor_exec(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fake_stage(root)
            stage = resolve_stage(
                root,
                bin_directory="bin",
                plugin_relative="lib/qindaqt/plugins/kwin/plugins/qindaqt_compositor.so",
                decoration_relative="lib/qindaqt/plugins/org.kde.kdecoration3/org.qindaqt.so",
                settings_service_directory="share/dbus-1/services",
                audio_service_directory="share/dbus-1/services",
            )
            self.assertEqual(stage.services["org.qindaqt.Settings1"].executable, root / "bin/qindaqt-settings-service")
            self.assertTrue(stage.services["org.qindaqt.Settings1"].activation_exec.startswith("/usr/bin/"))

    def test_symlink_and_escape_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fake_stage(root)
            target = root / "bin/qindaqt-settings"
            target.unlink()
            target.symlink_to(root / "bin/qindaqt-editor")
            with self.assertRaisesRegex(StageContractError, "symlink"):
                resolve_stage(
                    root,
                    bin_directory="bin",
                    plugin_relative="lib/qindaqt/plugins/kwin/plugins/qindaqt_compositor.so",
                    decoration_relative="lib/qindaqt/plugins/org.kde.kdecoration3/org.qindaqt.so",
                    settings_service_directory="share/dbus-1/services",
                    audio_service_directory="share/dbus-1/services",
                )
            with self.assertRaises(StageContractError):
                resolve_stage(
                    root,
                    bin_directory="../bin",
                    plugin_relative="lib/qindaqt/plugins/kwin/plugins/qindaqt_compositor.so",
                    decoration_relative="lib/qindaqt/plugins/org.kde.kdecoration3/org.qindaqt.so",
                    settings_service_directory="share/dbus-1/services",
                    audio_service_directory="share/dbus-1/services",
                )

    def test_stage_reset_requires_build_local_sentinel(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / "build"
            build.mkdir()
            stage = reset_stage_root(build / "stage", build)
            (stage / "payload").write_text("x")
            reset_stage_root(stage, build)
            foreign = build / "foreign"
            foreign.mkdir()
            with self.assertRaisesRegex(StageContractError, "sentinel"):
                reset_stage_root(foreign, build)


class SandboxTests(unittest.TestCase):
    def test_environment_is_an_allow_list(self) -> None:
        environment = sandbox_environment(
            run_id="a" * 32,
            uid=1000,
            stage_bin="/opt/qindaqt/bin",
            system_path=["/usr/bin"],
        )
        self.assertFalse(FORBIDDEN_ENVIRONMENT.intersection(environment))
        self.assertEqual(environment["DBUS_SESSION_BUS_ADDRESS"], "unix:path=/run/user/1000/bus")
        self.assertEqual(environment["PATH"], "/opt/qindaqt/bin:/usr/bin")

    def test_secondary_primary_selector_retains_private_plugin_precedence(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            backend = (
                Path(directory)
                / "host-plugins/kf6/kscreen/KSC_KWayland.so"
            )
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
            self.assertEqual(
                environment["QT_PLUGIN_PATH"], "/private/qt6/plugins"
            )

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
                "desktop_session_runtime.subprocess.Popen",
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

            libraries, plugin_roots, qml_roots = _library_search_roots(
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
        mounts = _system_mounts([Path(sys.executable)])
        cache = Path("/etc/ld.so.cache")
        cache_mounts = [mount for mount in mounts if mount.destination == PurePosixPath("/etc/ld.so.cache")]
        if cache.is_file():
            self.assertEqual(cache_mounts, [ReadOnlyMount(cache, PurePosixPath("/etc/ld.so.cache"))])
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
                _weston_module_map(weston),
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
            stage = root / "stage"
            source = root / "source"
            system = root / "system"
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
                    ReadOnlyMount(Path("/etc/ld.so.cache"), PurePosixPath("/etc/ld.so.cache")),
                ),
                environment=sandbox_environment(
                    run_id="b" * 32,
                    uid=1000,
                    stage_bin="/opt/qindaqt/bin",
                    system_path=["/usr/bin"],
                ),
                command=("/usr/bin/python3", "--version"),
            )
            argv = build_bwrap_argv(spec)
            for required in (
                "--unshare-pid",
                "--unshare-net",
                "--unshare-ipc",
                "--die-with-parent",
                "--new-session",
                "--clearenv",
            ):
                self.assertIn(required, argv)
            joined = " ".join(argv)
            self.assertNotIn("/dev/input", joined)
            self.assertNotIn("/dev/uinput", joined)
            self.assertNotIn("--bind / /", joined)
            self.assertNotIn("--ro-bind / /", joined)
            self.assertEqual(
                [
                    (argv[index + 1], argv[index + 2])
                    for index, value in enumerate(argv)
                    if value == "--ro-bind" and argv[index + 1].startswith("/etc")
                ],
                [("/etc/ld.so.cache", "/etc/ld.so.cache")],
            )
            self.assertNotIn("--ro-bind /etc /etc", joined)
            self.assertNotIn("--ro-bind /lib /lib", joined)
            self.assertNotIn("--ro-bind /lib64 /lib64", joined)
            aliases = [
                (argv[index + 1], argv[index + 2])
                for index, value in enumerate(argv)
                if value == "--symlink"
            ]
            self.assertEqual(aliases, [("usr/lib", "/lib"), ("usr/lib64", "/lib64")])
            self.assertLess(argv.index("/lib64"), argv.index("--proc"))
            remove_run_root(paths, build, "b" * 32)

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
            stage = root / "stage"
            source = root / "source"
            for item in (stage, source):
                item.mkdir()
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
                    run_id="c" * 32,
                    uid=os.getuid(),
                    stage_bin="/opt/qindaqt/bin",
                    system_path=["/usr/bin"],
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
                completed.returncode,
                0,
                msg=f"{completed.stdout}{completed.stderr}",
            )
            self.assertIn("Python", f"{completed.stdout}{completed.stderr}")
            remove_run_root(paths, build, "c" * 32)

    def test_run_root_cleanup_requires_exact_sentinel(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / "build"
            build.mkdir()
            paths = create_run_root(build, "c" * 32)
            paths.sentinel.write_text("wrong\n")
            with self.assertRaisesRegex(SandboxContractError, "sentinel"):
                remove_run_root(paths, build, "c" * 32)
            shutil.rmtree(paths.root)

    def test_cross_worktree_lock_is_nonblocking(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            lock = Path(directory) / "lane.lock"
            with PrivateLaneLock(lock):
                with self.assertRaisesRegex(SandboxContractError, "busy"):
                    with PrivateLaneLock(lock):
                        self.fail("second lock acquisition unexpectedly passed")


class ResultArchiveTests(unittest.TestCase):
    def test_fresh_result_copies_every_artifact_log_and_success_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / "build"
            build.mkdir()
            run_id = "d" * 32
            paths = create_run_root(build, run_id)
            result_root = create_result_root(build, run_id)
            (paths.artifacts / "evidence.json").write_text("{}\n")
            (paths.logs / "compositor.log").write_text("compositor\n")
            (paths.logs / "session-probe-001.log").write_text("probe\n")
            archive_attempt(
                paths, result_root, build, run_id, "sandbox\n",
                AttemptResult("success", 0, False, None, 10, 20),
            )
            self.assertEqual(
                sorted(path.name for path in (result_root / "logs").iterdir()),
                ["compositor.log", "session-probe-001.log"],
            )
            self.assertTrue((result_root / "artifacts/evidence.json").is_file())
            document = json.loads((result_root / "result.json").read_text())
            self.assertEqual(document["outcome"], "success")
            self.assertEqual(document["runId"], run_id)
            remove_run_root(paths, build, run_id)

    def test_stale_or_symlink_result_destination_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / "build"
            build.mkdir()
            stale_id = "e" * 32
            create_result_root(build, stale_id)
            with self.assertRaisesRegex(SandboxContractError, "already exists"):
                create_result_root(build, stale_id)
            symlink_id = "f" * 32
            target = Path(directory) / "outside"
            target.mkdir()
            (build / "tests/session/desktop-session-results" / symlink_id).symlink_to(
                target, target_is_directory=True
            )
            with self.assertRaisesRegex(SandboxContractError, "already exists"):
                create_result_root(build, symlink_id)

    def test_archive_rejects_a_tampered_source_run_identity(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / "build"
            build.mkdir()
            run_id = "a" * 32
            paths = create_run_root(build, run_id)
            result_root = create_result_root(build, run_id)
            paths.sentinel.write_text("wrong\n")
            with self.assertRaisesRegex(SandboxContractError, "sentinel"):
                archive_attempt(
                    paths, result_root, build, run_id, "",
                    AttemptResult("failure", 1, False, "cleanup", 10, 20),
                )
            shutil.rmtree(paths.root)

    def _run_fake_sandbox(
        self, root: Path, run_id: str, process: Mock
    ) -> tuple[int, dict[str, object], Path]:
        arguments = outer_arguments(root, run_id)

        def write_command(path: Path, _: object) -> None:
            path.write_text("{}\n")

        with (
            patch.dict(os.environ, {LANE_ENVIRONMENT: LANE_VALUE}),
            patch("test_desktop_session_nested._make_spec", return_value=Mock()),
            patch("test_desktop_session_nested.write_command_evidence", side_effect=write_command),
            patch("test_desktop_session_nested.build_bwrap_argv", return_value=["/bwrap"]),
            patch("test_desktop_session_nested.PrivateLaneLock", return_value=nullcontext()),
            patch("test_desktop_session_nested.subprocess.Popen", return_value=process),
            patch("test_desktop_session_nested.capture_process_identity", return_value=Mock()),
            patch("test_desktop_session_nested.terminate_processes", return_value=[]),
            redirect_stdout(StringIO()),
        ):
            status = run_outer(arguments)
        result_root = (
            arguments.build_root / "tests/session/desktop-session-results" / run_id
        )
        document = json.loads((result_root / "result.json").read_text())
        return status, document, arguments.build_root

    def test_timeout_is_archived_before_private_root_cleanup(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            process = Mock(pid=70, returncode=1)
            process.communicate.side_effect = [
                subprocess.TimeoutExpired(["/bwrap"], 55),
                ("partial output\n", None),
            ]
            status, document, build = self._run_fake_sandbox(
                Path(directory), "1" * 32, process
            )
            self.assertEqual(status, 1)
            self.assertEqual(document["outcome"], "timeout")
            self.assertTrue(document["timedOut"])
            self.assertFalse(
                (build / "tests/session/desktop-session-runs" / ("1" * 32)).exists()
            )

    def test_success_result_is_archived_before_private_root_cleanup(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            process = Mock(pid=71, returncode=0)
            process.communicate.return_value = ("complete\n", None)
            status, document, build = self._run_fake_sandbox(
                Path(directory), "3" * 32, process
            )
            self.assertEqual(status, 0)
            self.assertEqual(document["outcome"], "success")
            self.assertIsNone(document["failure"])
            self.assertFalse(
                (build / "tests/session/desktop-session-runs" / ("3" * 32)).exists()
            )

    def test_cleanup_failure_output_and_result_are_archived(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            process = Mock(pid=72, returncode=1)
            process.communicate.return_value = ("exact desktop cleanup failed\n", None)
            status, document, build = self._run_fake_sandbox(
                Path(directory), "2" * 32, process
            )
            self.assertEqual(status, 1)
            self.assertEqual(document["outcome"], "failure")
            result_root = build / "tests/session/desktop-session-results" / ("2" * 32)
            self.assertIn("exact desktop cleanup failed", (result_root / "sandbox.log").read_text())


if __name__ == "__main__":
    unittest.main()
