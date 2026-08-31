# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile mutations for the DesktopVirtual Network import closure."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from desktop_session_package_contract import (
    NETWORK_QML_FILES,
    PackagePayloadError,
    authenticate_network_qml_package,
)


QML_DIRECTORY = "lib64/qt6/qml"
LIBRARY = "libqindaqt_settings_network_qml.so"
PLUGIN = "libqindaqt_settings_network_qmlplugin.so"
REQUIRED = (
    LIBRARY,
    PLUGIN,
    "qmldir",
    "qindaqt_settings_network.qmltypes",
    *NETWORK_QML_FILES,
)


def network_module(stage: Path, *, missing: str = "") -> Path:
    module = stage / QML_DIRECTORY / "QindaQt/SettingsApp/Network"
    module.mkdir(parents=True)
    for relative in REQUIRED:
        if relative == missing:
            continue
        path = module / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("fixture\n", encoding="utf-8")
    return module


class PackageContractTests(unittest.TestCase):
    def test_exact_network_qml_closure_passes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory) / "stage"
            stage.mkdir()
            network_module(stage)
            artifacts = authenticate_network_qml_package(
                stage,
                qml_directory=QML_DIRECTORY,
                library_name=LIBRARY,
                plugin_name=PLUGIN,
            )
            self.assertEqual(len(artifacts), len(REQUIRED))

    def test_each_missing_network_artifact_fails_closed(self) -> None:
        for missing in REQUIRED:
            with self.subTest(missing=missing), tempfile.TemporaryDirectory() as directory:
                stage = Path(directory) / "stage"
                stage.mkdir()
                network_module(stage, missing=missing)
                with self.assertRaisesRegex(PackagePayloadError, "omitted"):
                    authenticate_network_qml_package(
                        stage,
                        qml_directory=QML_DIRECTORY,
                        library_name=LIBRARY,
                        plugin_name=PLUGIN,
                    )

    def test_network_payload_rejects_symlink_and_traversal(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory) / "stage"
            stage.mkdir()
            module = network_module(stage, missing="qml/NetworkPage.qml")
            outside = Path(directory) / "outside.qml"
            outside.write_text("fixture\n", encoding="utf-8")
            (module / "qml/NetworkPage.qml").symlink_to(outside)
            with self.assertRaisesRegex(PackagePayloadError, "regular file"):
                authenticate_network_qml_package(
                    stage,
                    qml_directory=QML_DIRECTORY,
                    library_name=LIBRARY,
                    plugin_name=PLUGIN,
                )
            with self.assertRaisesRegex(PackagePayloadError, "normalized"):
                authenticate_network_qml_package(
                    stage,
                    qml_directory="../outside",
                    library_name=LIBRARY,
                    plugin_name=PLUGIN,
                )


if __name__ == "__main__":
    unittest.main()
