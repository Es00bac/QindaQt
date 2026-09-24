#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused pure tests for the install-checkpoint parser and rollback plan."""

from __future__ import annotations

from pathlib import Path
import struct
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SCRIPT_DIR = ROOT / "tools/install-checkpoint"
sys.path.insert(0, str(SCRIPT_DIR))

import checkpoint_audit as audit  # noqa: E402
import checkpoint_contract as contract  # noqa: E402
import checkpoint_install as install  # noqa: E402
import checkpoint_rollback as rollback  # noqa: E402
import checkpoint_stage as stage  # noqa: E402


class SnapshotParsingTests(unittest.TestCase):
    def test_audio_snapshot_reads_schema_without_parsing_inventory(self) -> None:
        reply = "(uttuuss(tt)) 12 4 9 1 0 \"private-device-name\""
        self.assertEqual(audit.parse_audio_snapshot_reply(reply), 12)

    def test_audio_snapshot_rejects_unexpected_signature(self) -> None:
        with self.assertRaisesRegex(ValueError, "unexpected signature"):
            audit.parse_audio_snapshot_reply("s \"not a snapshot\"")

    def test_network_snapshot_reads_codec_and_protocol_header(self) -> None:
        payload = b"QN1S" + struct.pack(">II", 1, 1) + b"rest"
        reply = "ay " + str(len(payload)) + " " + " ".join(str(value) for value in payload)
        self.assertEqual(audit.parse_network_snapshot_reply(reply), (1, 1))

    def test_network_snapshot_rejects_truncated_or_unknown_header(self) -> None:
        with self.assertRaisesRegex(ValueError, "truncated"):
            audit.parse_network_snapshot_reply("ay 2 1 2")
        payload = b"NOPE" + struct.pack(">II", 1, 1)
        reply = "ay " + str(len(payload)) + " " + " ".join(str(value) for value in payload)
        with self.assertRaisesRegex(ValueError, "unknown header"):
            audit.parse_network_snapshot_reply(reply)


class ReadOnlyAuditTests(unittest.TestCase):
    def test_live_version_query_uses_only_existing_owner_and_read_method(self) -> None:
        calls: list[list[str]] = []

        def capture(argv: list[str], timeout: float = 8.0) -> subprocess.CompletedProcess[str]:
            calls.append(argv)
            output = ':1.42\n' if len(calls) == 1 else '(uttuuss(tt)) 12 1 2'
            return subprocess.CompletedProcess(argv, 0, output, "")

        with mock.patch.object(audit, "run_capture", side_effect=capture):
            version = audit.live_bus_version(contract.SERVICES["Audio1"])

        self.assertEqual(version["version"], {"schemaVersion": 12})
        self.assertIn("GetNameOwner", calls[0])
        self.assertIn("--auto-start=no", calls[1])
        self.assertIn(":1.42", calls[1])
        self.assertEqual(calls[1][-1], "GetSnapshot")

    def test_state_paths_follow_xdg_without_reading_profile_contents(self) -> None:
        paths = audit.state_locations({
            "HOME": "/tmp/private-home",
            "XDG_CONFIG_HOME": "/tmp/private-config",
            "XDG_DATA_HOME": "/tmp/private-data",
        })
        self.assertEqual(str(paths["settings1"]), "/tmp/private-config/qindaqt/settings-v2.json")
        self.assertEqual(str(paths["audioConsole"]), "/tmp/private-config/qindaqt/audio-console.json")
        self.assertEqual(str(paths["networkManagerProfiles"]), "/etc/NetworkManager/system-connections")

    def test_repository_ebuild_lookup_uses_installed_vdb_repository_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            package = Path(temp) / "qindaqt-desktop-1"
            package.mkdir()
            (package / "repository").write_text("qindaqt\n")
            self.assertEqual(audit.package_repository_name(package), "qindaqt")
            self.assertEqual(
                audit.package_atom("gui-wm", package.name, "qindaqt"),
                "=gui-wm/qindaqt-desktop-1::qindaqt",
            )
            result = subprocess.CompletedProcess(
                ["portageq"], 0, "/var/db/repos/qindaqt\n", ""
            )
            with mock.patch.object(audit, "run_capture", return_value=result) as capture:
                ebuild = audit.repository_ebuild_path(
                    audit.package_repository_name(package), "gui-wm", package.name
                )
            capture.assert_called_once_with(
                ["portageq", "get_repo_path", "/", "qindaqt"]
            )
            self.assertEqual(
                ebuild,
                Path("/var/db/repos/qindaqt/gui-wm/qindaqt-desktop")
                / f"{package.name}.ebuild",
            )
            self.assertIsNone(audit.repository_ebuild_path(None, "gui-wm", package.name))

    def test_desktop_parser_reports_settings_actions(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "org.qindaqt.Settings.desktop"
            path.write_text(
                "[Desktop Entry]\nExec=qindaqt-settings\nActions=appearance;display;\n"
                "[Desktop Action appearance]\nExec=qindaqt-settings --page appearance\n"
                "[Desktop Action display]\nExec=qindaqt-settings --page display\n"
            )
            entry = audit.desktop_entry(path)
            self.assertEqual(entry["exec"], "qindaqt-settings")
            self.assertEqual(entry["actions"], ["appearance", "display"])
            self.assertEqual(entry["actionExecs"]["display"], "qindaqt-settings --page display")

    def test_staged_session_entry_checks_exec_tryexec_and_binary(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            entry = root / "qindaqt.desktop"
            binary = root / "usr/bin/qindaqt-wm"
            binary.parent.mkdir(parents=True)
            binary.write_text("probe")
            binary.chmod(0o755)
            entry.write_text(
                "[Desktop Entry]\nExec=/usr/bin/qindaqt-wm --drm\n"
                "TryExec=/usr/bin/qindaqt-wm\n"
            )
            parsed = install.validate_session_entry(entry, binary)
            self.assertEqual(parsed["exec"], contract.SESSION_EXEC)
            self.assertEqual(parsed["tryExec"], contract.SESSION_TRY_EXEC)
            entry.write_text(
                "[Desktop Entry]\nExec=/usr/bin/qindaqt-shell\n"
                "TryExec=/usr/bin/qindaqt-wm\n"
            )
            with self.assertRaisesRegex(RuntimeError, "session Exec"):
                install.validate_session_entry(entry, binary)
            entry.write_text(
                "[Desktop Entry]\nExec=/usr/bin/qindaqt-wm --drm\n"
                "TryExec=/usr/bin/qindaqt-shell\n"
            )
            with self.assertRaisesRegex(RuntimeError, "session TryExec"):
                install.validate_session_entry(entry, binary)

    def test_generated_absolute_etc_rule_maps_under_destdir_without_live_write(self) -> None:
        live_entry = Path("/etc/xdg/autostart/qindaqt-obs-login.desktop")
        before = (live_entry.exists(), audit.sha256_file(live_entry))
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            source = root / "source"
            build = repo / "build/cmake"
            stage_root = repo / "build/stage"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            (repo / ".gitignore").write_text("/build/\n")
            source.mkdir()
            (source / "qindaqt-obs-login.desktop").write_text("[Desktop Entry]\nName=probe\n")
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(InstallDestinationProbe NONE)\n"
                "install(FILES \"${CMAKE_CURRENT_SOURCE_DIR}/qindaqt-obs-login.desktop\" "
                "DESTINATION \"/etc/xdg/autostart\")\n"
                "install(FILES \"${CMAKE_CURRENT_SOURCE_DIR}/qindaqt-obs-login.desktop\" "
                "DESTINATION \"${CMAKE_INSTALL_PREFIX}/share/qindaqt-probe\")\n"
            )
            subprocess.run(
                [
                    "cmake",
                    "-S",
                    str(source),
                    "-B",
                    str(build),
                    "-DCMAKE_INSTALL_PREFIX=/usr",
                ],
                check=True,
                capture_output=True,
                text=True,
            )
            audit.validate_build_paths(repo, build, stage_root)
            report = install.audit_cmake_install_scripts(build, stage_root, Path("/usr"))
            self.assertEqual(report["cmakeInstallScriptCount"], 1)
            self.assertEqual(report["installRuleCount"], 2)
            self.assertEqual(report["installPrefixResolvedRuleCount"], 0)
            self.assertGreaterEqual(
                report["otherGeneratedSideEffects"]["buildTreeWrites"], 1
            )
            destinations = {
                rule["destination"]: Path(rule["stagedDestination"])
                for rule in report["literalAbsoluteDestinationRules"]
            }
            self.assertEqual(
                destinations,
                {
                    "/etc/xdg/autostart": stage_root / "etc/xdg/autostart",
                    "/usr/share/qindaqt-probe": stage_root / "usr/share/qindaqt-probe",
                },
            )
            self.assertTrue(
                all(path.is_relative_to(stage_root) for path in destinations.values())
            )
            install.validate_staged_paths(
                stage_root,
                [stage_root / "etc/xdg/autostart/qindaqt-obs-login.desktop", stage_root / "usr/share/qindaqt-probe"],
            )
            with self.assertRaisesRegex(RuntimeError, "outside the DESTDIR root"):
                install.validate_staged_paths(stage_root, [live_entry])
        self.assertEqual((live_entry.exists(), audit.sha256_file(live_entry)), before)

    def test_generated_rpath_checks_and_changes_use_destdir_rooted_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            source = root / "source"
            build = repo / "build/cmake"
            stage_root = repo / "build/stage"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            (repo / ".gitignore").write_text("/build/\n")
            source.mkdir()
            (source / "probe.c").write_text("int qindaqt_probe(void) { return 0; }\n")
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(RpathInstallProbe C)\n"
                "add_library(rpath_probe SHARED probe.c)\n"
                "set_target_properties(rpath_probe PROPERTIES "
                "BUILD_RPATH \"/tmp/checkpoint-build-rpath\" INSTALL_RPATH \"$ORIGIN\")\n"
                "install(TARGETS rpath_probe LIBRARY DESTINATION lib)\n"
            )
            subprocess.run(
                ["cmake", "-S", str(source), "-B", str(build), "-DCMAKE_INSTALL_PREFIX=/usr"],
                check=True,
                capture_output=True,
                text=True,
            )
            audit.validate_build_paths(repo, build, stage_root)
            report = install.audit_cmake_install_scripts(build, stage_root, Path("/usr"))
            self.assertGreaterEqual(report["otherGeneratedSideEffects"]["rpathChecks"], 2)
            self.assertGreaterEqual(report["otherGeneratedSideEffects"]["destdirStripCommands"], 1)

    def test_custom_install_code_write_outside_build_tree_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            source = root / "source"
            build = repo / "build/cmake"
            stage_root = repo / "build/stage"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            (repo / ".gitignore").write_text("/build/\n")
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(CustomInstallCodeProbe NONE)\n"
                "install(CODE [=[file(WRITE \"/etc/xdg/autostart/unsafe.desktop\" \"unsafe\")]=])\n"
            )
            subprocess.run(
                ["cmake", "-S", str(source), "-B", str(build), "-DCMAKE_INSTALL_PREFIX=/usr"],
                check=True,
                capture_output=True,
                text=True,
            )
            audit.validate_build_paths(repo, build, stage_root)
            with self.assertRaisesRegex(ValueError, "file write is outside the ignored build tree"):
                install.audit_cmake_install_scripts(build, stage_root, Path("/usr"))

    def test_custom_install_process_targeting_live_etc_fails_before_execution(self) -> None:
        live_entry = Path("/etc/xdg/autostart/qindaqt-obs-login.desktop")
        before = (live_entry.exists(), audit.sha256_file(live_entry))
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            source = root / "source"
            build = repo / "build/cmake"
            stage_root = repo / "build/stage"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            (repo / ".gitignore").write_text("/build/\n")
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(CustomInstallProcessProbe NONE)\n"
                "install(CODE [=[execute_process(COMMAND \"/usr/bin/touch\" "
                "\"/etc/xdg/autostart/qindaqt-obs-login.desktop\")]=])\n"
            )
            subprocess.run(
                ["cmake", "-S", str(source), "-B", str(build), "-DCMAKE_INSTALL_PREFIX=/usr"],
                check=True,
                capture_output=True,
                text=True,
            )
            audit.validate_build_paths(repo, build, stage_root)
            with self.assertRaisesRegex(ValueError, "unsupported generated install process"):
                install.audit_cmake_install_scripts(build, stage_root, Path("/usr"))
        self.assertEqual((live_entry.exists(), audit.sha256_file(live_entry)), before)

    def test_custom_install_code_cannot_clear_destdir(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            source = root / "source"
            build = repo / "build/cmake"
            stage_root = repo / "build/stage"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            (repo / ".gitignore").write_text("/build/\n")
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(CustomInstallDestdirProbe NONE)\n"
                "install(CODE [=[set(ENV{DESTDIR} \"\")]=])\n"
            )
            subprocess.run(
                ["cmake", "-S", str(source), "-B", str(build), "-DCMAKE_INSTALL_PREFIX=/usr"],
                check=True,
                capture_output=True,
                text=True,
            )
            audit.validate_build_paths(repo, build, stage_root)
            with self.assertRaisesRegex(ValueError, "changes DESTDIR"):
                install.audit_cmake_install_scripts(build, stage_root, Path("/usr"))

    def test_custom_install_code_cannot_mutate_prefix_via_list(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            source = root / "source"
            build = repo / "build/cmake"
            stage_root = repo / "build/stage"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            (repo / ".gitignore").write_text("/build/\n")
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(CustomInstallPrefixProbe NONE)\n"
                "install(CODE [=[list(APPEND CMAKE_INSTALL_PREFIX \"/etc\")]=])\n"
            )
            subprocess.run(
                ["cmake", "-S", str(source), "-B", str(build), "-DCMAKE_INSTALL_PREFIX=/usr"],
                check=True,
                capture_output=True,
                text=True,
            )
            audit.validate_build_paths(repo, build, stage_root)
            with self.assertRaisesRegex(ValueError, r"list\(\) mutation"):
                install.audit_cmake_install_scripts(build, stage_root, Path("/usr"))

    def test_custom_install_script_outside_generated_tree_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            source = root / "source"
            build = repo / "build/cmake"
            stage_root = repo / "build/stage"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            (repo / ".gitignore").write_text("/build/\n")
            source.mkdir()
            (source / "unsafe-install.cmake").write_text(
                "file(WRITE \"/etc/xdg/autostart/unsafe.desktop\" \"unsafe\")\n"
            )
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(CustomInstallScriptProbe NONE)\n"
                "install(SCRIPT \"${CMAKE_CURRENT_SOURCE_DIR}/unsafe-install.cmake\")\n"
            )
            subprocess.run(
                ["cmake", "-S", str(source), "-B", str(build), "-DCMAKE_INSTALL_PREFIX=/usr"],
                check=True,
                capture_output=True,
                text=True,
            )
            audit.validate_build_paths(repo, build, stage_root)
            with self.assertRaisesRegex(ValueError, "outside the build tree"):
                install.audit_cmake_install_scripts(build, stage_root, Path("/usr"))

    def test_generated_build_local_install_script_is_audited_for_writes(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            source = root / "source"
            build = repo / "build/cmake"
            stage_root = repo / "build/stage"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            (repo / ".gitignore").write_text("/build/\n")
            source.mkdir()
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\n"
                "project(GeneratedInstallScriptProbe NONE)\n"
                "file(GENERATE OUTPUT \"${CMAKE_CURRENT_BINARY_DIR}/unsafe-install.cmake\" "
                "CONTENT \"file(WRITE \\\"/etc/xdg/autostart/unsafe.desktop\\\" \\\"unsafe\\\")\\n\")\n"
                "install(SCRIPT \"${CMAKE_CURRENT_BINARY_DIR}/unsafe-install.cmake\")\n"
            )
            subprocess.run(
                ["cmake", "-S", str(source), "-B", str(build), "-DCMAKE_INSTALL_PREFIX=/usr"],
                check=True,
                capture_output=True,
                text=True,
            )
            audit.validate_build_paths(repo, build, stage_root)
            with self.assertRaisesRegex(ValueError, "file write is outside the ignored build tree"):
                install.audit_cmake_install_scripts(build, stage_root, Path("/usr"))

    def test_missing_optional_include_is_allowed_only_under_build_tree(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "build"
            build.mkdir()
            script = build / "cmake_install.cmake"
            optional_inside = build / "missing-optional.cmake"
            script.write_text(f'include("{optional_inside}" OPTIONAL)\n')
            counts = install.audit_generated_install_effects(
                [script], build, build / "stage", Path("/usr")
            )
            self.assertEqual(counts["missingOptionalIncludes"], 1)

            optional_outside = root / "outside/missing-optional.cmake"
            script.write_text(f'include("{optional_outside}" OPTIONAL)\n')
            with self.assertRaisesRegex(ValueError, "outside the build tree"):
                install.audit_generated_install_effects(
                    [script], build, build / "stage", Path("/usr")
                )

    def test_stage_path_guard_allows_only_ignored_build_descendants(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            repo = Path(temp)
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            (repo / ".gitignore").write_text("/build/\n")
            build = repo / "build/checkpoint/cmake"
            prefix = repo / "build/checkpoint/stage"
            build.mkdir(parents=True)
            audit.validate_build_paths(repo, build, prefix)
            with self.assertRaisesRegex(ValueError, "beneath ignored"):
                audit.validate_build_paths(repo, build, Path(temp) / "live")

    def test_generated_write_through_build_symlink_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "repo/build/cmake"
            outside = root / "outside"
            build.mkdir(parents=True)
            outside.mkdir()
            (build / "escape").symlink_to(outside, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "outside the ignored build tree"):
                install._validate_build_write_target(
                    str(build / "escape/unsafe.txt"), build.resolve(), build / "cmake_install.cmake"
                )

    def test_generated_install_audit_rejects_symlinked_script_directories(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo = root / "repo"
            build = repo / "build/cmake"
            outside = root / "external"
            repo.mkdir()
            subprocess.run(["git", "init", "-q", str(repo)], check=True)
            (repo / ".gitignore").write_text("/build/\n")
            build.mkdir(parents=True)
            outside.mkdir()
            (build / "cmake_install.cmake").write_text("# generated script inventory root\n")
            (outside / "cmake_install.cmake").write_text(
                'file(INSTALL DESTINATION "/etc" TYPE FILE FILES "/tmp/source")\n'
            )
            (build / "linked").symlink_to(outside, target_is_directory=True)
            stage_root = repo / "build/stage"
            audit.validate_build_paths(repo, build, stage_root)
            with self.assertRaisesRegex(ValueError, "symlinked directories"):
                install.audit_cmake_install_scripts(build, stage_root, Path("/usr"))


class StageLogTests(unittest.TestCase):
    def test_build_regeneration_is_followed_by_audit_before_install(self) -> None:
        events: list[str] = []
        final_audit = {"cmakeInstallScriptCount": 2, "installRuleCount": 3}

        def fake_run_logged(label: str, *_args: object, **_kwargs: object) -> None:
            events.append(label)

        def fake_cache(*_args: object, **_kwargs: object) -> dict[str, str]:
            events.append("post-build cache check")
            return {"CMAKE_INSTALL_PREFIX": "/usr"}

        def fake_audit(*_args: object, **_kwargs: object) -> dict[str, int]:
            events.append("post-build audit")
            return final_audit

        def fake_install_invocation(*_args: object, **_kwargs: object) -> tuple[list[str], dict[str, str]]:
            events.append("install invocation")
            return ["cmake", "--install"], {"DESTDIR": "/tmp/checkpoint/stage"}

        with (
            mock.patch.object(stage, "run_logged", side_effect=fake_run_logged),
            mock.patch.object(stage, "cmake_cache", side_effect=fake_cache),
            mock.patch.object(stage, "audit_cmake_install_scripts", side_effect=fake_audit),
            mock.patch.object(stage, "staged_install_invocation", side_effect=fake_install_invocation),
        ):
            result = stage.build_then_install_with_final_audit(
                Path("/tmp/checkpoint/cmake"),
                Path("/tmp/checkpoint/stage"),
                ["cmake", "--build"],
                Path("/tmp/checkpoint/build.log"),
                Path("/tmp/checkpoint/install.log"),
            )

        self.assertEqual(
            events,
            [
                "production build",
                "post-build cache check",
                "post-build audit",
                "install invocation",
                "staged install",
            ],
        )
        self.assertEqual(result[0], final_audit)
        self.assertEqual(result[1], ["cmake", "--install"])

    def test_post_build_prefix_change_aborts_before_audit_or_install(self) -> None:
        events: list[str] = []

        def fake_run_logged(label: str, *_args: object, **_kwargs: object) -> None:
            events.append(label)

        def fake_cache(*_args: object, **_kwargs: object) -> dict[str, str]:
            events.append("post-build cache check")
            return {"CMAKE_INSTALL_PREFIX": "/"}

        with (
            mock.patch.object(stage, "run_logged", side_effect=fake_run_logged),
            mock.patch.object(stage, "cmake_cache", side_effect=fake_cache),
            mock.patch.object(stage, "audit_cmake_install_scripts") as audit_scripts,
            mock.patch.object(stage, "staged_install_invocation") as install_invocation,
        ):
            with self.assertRaisesRegex(RuntimeError, "post-build CMake install prefix"):
                stage.build_then_install_with_final_audit(
                    Path("/tmp/checkpoint/cmake"),
                    Path("/tmp/checkpoint/stage"),
                    ["cmake", "--build"],
                    Path("/tmp/checkpoint/build.log"),
                    Path("/tmp/checkpoint/install.log"),
                )

        self.assertEqual(events, ["production build", "post-build cache check"])
        audit_scripts.assert_not_called()
        install_invocation.assert_not_called()

    def test_run_logged_keeps_build_output_out_of_the_console(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            log_path = Path(temp) / "logs/build.log"
            observed: list[dict[str, object]] = []

            def fake_run(command: list[str], **kwargs: object) -> subprocess.CompletedProcess[str]:
                observed.append(kwargs)
                kwargs["stdout"].write("compile complete\n")  # type: ignore[union-attr]
                return subprocess.CompletedProcess(command, 0, "", "")

            with mock.patch.object(stage.subprocess, "run", side_effect=fake_run):
                stage.run_logged("production build", ["cmake", "--build"], log_path)

            self.assertEqual(log_path.read_text(), "compile complete\n")
            self.assertEqual(observed[0]["stderr"], subprocess.STDOUT)
            self.assertEqual(observed[0]["text"], True)

    def test_build_and_focused_tests_use_separate_cmake_composition(self) -> None:
        self.assertIn("-DBUILD_TESTING=OFF", contract.CMAKE_OPTIONS)
        self.assertNotIn("-DBUILD_TESTING=ON", contract.CMAKE_OPTIONS)
        self.assertIn("-DQINDAQT_ENABLE_STRICT_WARNINGS=OFF", contract.CMAKE_OPTIONS)

    def test_install_invocation_uses_destdir_and_never_overrides_usr_prefix(self) -> None:
        build = Path("/tmp/checkpoint/cmake")
        stage_root = Path("/tmp/checkpoint/stage")
        command, environment = stage.staged_install_invocation(
            build, stage_root, {"PATH": "/usr/bin", "DESTDIR": "/unsafe"}
        )
        self.assertEqual(command, ["cmake", "--install", str(build)])
        self.assertNotIn("--prefix", command)
        self.assertEqual(environment["DESTDIR"], str(stage_root.resolve()))
        self.assertEqual(environment["PATH"], "/usr/bin")


class RollbackScriptTests(unittest.TestCase):
    def test_snapshot_script_is_explicit_and_excludes_network_profiles(self) -> None:
        script = rollback.render_snapshot_script(
            [Path("/home/test/.config/qindaqt/audio-console.json")],
            Path("/tmp/checkpoint/state.tar"),
            Path("/tmp/checkpoint/state.sha256"),
        )
        self.assertIn(contract.SNAPSHOT_CONFIRMATION, script)
        self.assertIn("audio-console.json", script)
        self.assertNotIn("NetworkManager/system-connections", script)
        self.assertEqual(subprocess.run(["sh", "-n"], input=script, text=True).returncode, 0)

    def test_rollback_restores_exact_package_and_only_active_units(self) -> None:
        package = {
            "rollbackAvailable": True,
            "atom": "=gui-wm/qindaqt-desktop-0.1.0_pre20260923-r2::qindaqt",
            "repositoryEbuild": "/var/db/repos/qindaqt/qindaqt.ebuild",
            "repositoryEbuildSha256": "b" * 64,
            "sourceArchive": "/var/cache/distfiles/qindaqt.tar.gz",
            "sourceArchiveSha256": "a" * 64,
        }
        script = rollback.render_rollback_script(
            package,
            ["qindaqt-audio-service.service", "qindaqt-network-service.service"],
            Path("/tmp/checkpoint/state.tar"),
            Path("/tmp/checkpoint/state.sha256"),
            socket.gethostname(),
        )
        self.assertIn(contract.ROLLBACK_CONFIRMATION, script)
        self.assertIn("sudo emerge --ask --oneshot =gui-wm/qindaqt-desktop-0.1.0_pre20260923-r2::qindaqt", script)
        self.assertIn("expected_ebuild_sha", script)
        self.assertIn("Prior repository ebuild hash changed", script)
        self.assertIn(f"expected_host={socket.gethostname()}", script)
        self.assertIn("systemctl --user stop qindaqt-audio-service.service", script)
        self.assertIn("systemctl --user start qindaqt-network-service.service", script)
        self.assertIn("trap restart_services EXIT", script)
        self.assertIn("trap 'exit 143' TERM", script)
        self.assertLess(
            script.index("sudo emerge --ask --oneshot"),
            script.index("systemctl --user stop qindaqt-audio-service.service"),
        )
        self.assertIn("tar -xpf", script)
        self.assertNotIn("NetworkManager/system-connections", script)
        self.assertEqual(subprocess.run(["sh", "-n"], input=script, text=True).returncode, 0)

    def test_rollback_stops_before_mutation_when_prior_ebuild_hash_changes(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ebuild = root / "qindaqt.ebuild"
            archive = root / "qindaqt.tar.gz"
            snapshot = root / "state.tar"
            checksum = root / "state.sha256"
            ebuild.write_text("changed recipe")
            archive.write_text("prior source")
            snapshot.write_text("saved state")
            checksum.write_text(f"{audit.sha256_file(snapshot)}  {snapshot}\n")
            package = {
                "rollbackAvailable": True,
                "atom": "=gui-wm/qindaqt-desktop-0.1.0_pre20260923-r2::qindaqt",
                "repositoryEbuild": str(ebuild),
                "repositoryEbuildSha256": "b" * 64,
                "sourceArchive": str(archive),
                "sourceArchiveSha256": audit.sha256_file(archive),
            }
            script = rollback.render_rollback_script(
                package, [], snapshot, checksum, socket.gethostname()
            )
            result = subprocess.run(
                ["sh", "-c", script, "rollback", contract.ROLLBACK_CONFIRMATION],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 66)
            self.assertIn("Prior repository ebuild hash changed", result.stderr)
            self.assertNotIn("emerge", result.stdout + result.stderr)

    def test_rollback_stops_before_mutation_when_source_archive_hash_changes(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            ebuild = root / "qindaqt.ebuild"
            archive = root / "qindaqt.tar.gz"
            snapshot = root / "state.tar"
            checksum = root / "state.sha256"
            ebuild.write_text("prior recipe")
            archive.write_text("changed source")
            snapshot.write_text("saved state")
            checksum.write_text(f"{audit.sha256_file(snapshot)}  {snapshot}\n")
            package = {
                "rollbackAvailable": True,
                "atom": "=gui-wm/qindaqt-desktop-0.1.0_pre20260923-r2::qindaqt",
                "repositoryEbuild": str(ebuild),
                "repositoryEbuildSha256": audit.sha256_file(ebuild),
                "sourceArchive": str(archive),
                "sourceArchiveSha256": "a" * 64,
            }
            script = rollback.render_rollback_script(
                package, [], snapshot, checksum, socket.gethostname()
            )
            result = subprocess.run(
                ["sh", "-c", script, "rollback", contract.ROLLBACK_CONFIRMATION],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 66)
            self.assertIn("Prior source archive hash changed", result.stderr)
            self.assertNotIn("emerge", result.stdout + result.stderr)

    def test_rollback_recipe_refuses_to_run_on_a_different_host(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fake_bin = root / "bin"
            fake_bin.mkdir()
            hostname = fake_bin / "hostname"
            hostname.write_text("#!/bin/sh\nprintf 'target-host\\n'\n")
            hostname.chmod(0o755)
            package = {
                "rollbackAvailable": True,
                "atom": "=gui-wm/qindaqt-desktop-0.1.0_pre20260923-r2::qindaqt",
                "repositoryEbuild": "/unused/qindaqt.ebuild",
                "repositoryEbuildSha256": "b" * 64,
                "sourceArchive": "/unused/qindaqt.tar.gz",
                "sourceArchiveSha256": "a" * 64,
            }
            script = rollback.render_rollback_script(
                package,
                [],
                root / "state.tar",
                root / "state.sha256",
                "build-host",
            )
            result = subprocess.run(
                ["sh", "-c", script, "rollback", contract.ROLLBACK_CONFIRMATION],
                env={"PATH": f"{fake_bin}:/usr/bin:/bin"},
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 66)
            self.assertIn("belongs to host build-host, not target-host", result.stderr)
            self.assertNotIn("emerge", result.stdout + result.stderr)

    def test_rollback_refuses_when_prior_artifacts_are_not_available(self) -> None:
        with self.assertRaisesRegex(ValueError, "exact prior Portage package"):
            rollback.render_rollback_script(
                {}, [], Path("/tmp/state.tar"), Path("/tmp/state.sha256"), "test-host"
            )


if __name__ == "__main__":
    unittest.main()
