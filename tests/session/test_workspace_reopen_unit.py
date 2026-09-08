# SPDX-License-Identifier: GPL-3.0-or-later
"""Unit coverage for the workspace-reopen driver fixture and evidence logic."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from workspace_reopen_fixture import (
    EDITOR_APP_ID,
    PHANTOM_APP_ID,
    TERMINAL_APP_ID,
    clear_ksycoca,
    desktop_entry_text,
    extract_reopen_result,
    load_workspace_documents,
    nested_environment_variables,
    remove_fixture_application,
    store_directory,
    validate_reopen_evidence,
    validate_save_evidence,
    write_fixture_applications,
)


def _fixture_document(**overrides):
    document = {
        "schemaVersion": 1,
        "id": "fixture-id-1",
        "name": "cvn",
        "color": "#30A46C",
        "layout": {"pages": []},
        "applications": [
            {"id": "slot-1", "label": "One", "desktopEntryId": TERMINAL_APP_ID, "urls": []},
            {"id": "slot-2", "label": "Two", "desktopEntryId": TERMINAL_APP_ID, "urls": []},
            {"id": "slot-3", "label": "P", "desktopEntryId": PHANTOM_APP_ID, "urls": []},
        ],
    }
    document.update(overrides)
    return document


def _save_result(document):
    return {"phase": "save", "workspaceId": document["id"], "containerId": "c1"}


def _reopen_result(document, **overrides):
    result = {
        "phase": "reopen",
        "workspaceId": document["id"],
        "containerId": "c2",
        "layoutMatchesSavedDocument": True,
        "duplicateSlotsExplicitlyAssigned": True,
        "nameColorRoundTripThroughRestore": True,
        "missingApplicationReported": PHANTOM_APP_ID,
        "replacementApplication": EDITOR_APP_ID,
    }
    result.update(overrides)
    return result


class DesktopEntryFixtureTest(unittest.TestCase):
    def test_entry_text_is_parseable_and_safe(self):
        text = desktop_entry_text(TERMINAL_APP_ID, "WReopen Terminal")
        self.assertIn("[Desktop Entry]", text)
        self.assertIn("Type=Application", text)
        self.assertIn("Name=WReopen Terminal", text)
        with self.assertRaises(ValueError):
            desktop_entry_text("bad\nid", "name")

    def test_write_and_remove_fixture_applications(self):
        with tempfile.TemporaryDirectory() as directory:
            write_fixture_applications(directory)
            applications = Path(directory) / "applications"
            self.assertEqual(
                sorted(path.name for path in applications.glob("*.desktop")),
                sorted(f"{app_id}.desktop" for app_id in (TERMINAL_APP_ID, PHANTOM_APP_ID, EDITOR_APP_ID)),
            )
            remove_fixture_application(directory, PHANTOM_APP_ID)
            self.assertFalse((applications / f"{PHANTOM_APP_ID}.desktop").exists())
            self.assertTrue((applications / f"{TERMINAL_APP_ID}.desktop").exists())
            with self.assertRaises(RuntimeError):
                remove_fixture_application(directory, PHANTOM_APP_ID)

    def test_clear_ksycoca_only_removes_cache_files(self):
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            (cache / "ksycoca6").write_text("cache", encoding="utf-8")
            (cache / "ksycoca6_en").write_text("cache", encoding="utf-8")
            (cache / "other").write_text("keep", encoding="utf-8")
            self.assertEqual(clear_ksycoca(directory), 2)
            self.assertTrue((cache / "other").exists())
            self.assertEqual(clear_ksycoca(directory), 0)


class DocumentStoreTest(unittest.TestCase):
    def test_load_workspace_documents_reads_sorted_documents(self):
        with tempfile.TemporaryDirectory() as directory:
            store = store_directory(directory)
            store.mkdir(parents=True)
            (store / "b.json").write_text(json.dumps({"id": "b"}), encoding="utf-8")
            (store / "a.json").write_text(json.dumps({"id": "a"}), encoding="utf-8")
            self.assertEqual(
                [document["id"] for document in load_workspace_documents(directory)],
                ["a", "b"],
            )
            self.assertEqual(load_workspace_documents(str(Path(directory) / "none")), [])


class EvidenceValidationTest(unittest.TestCase):
    def test_extract_reopen_result_requires_exact_marker_and_phase(self):
        result = {"phase": "save", "workspaceId": "x"}
        stdout = f"noise\nQINDAQT_WORKSPACE_REOPEN={json.dumps(result)}\n"
        self.assertEqual(extract_reopen_result(stdout, "save"), result)
        with self.assertRaises(RuntimeError):
            extract_reopen_result("no marker here", "save")
        with self.assertRaises(RuntimeError):
            extract_reopen_result(stdout, "reopen")

    def test_validate_save_evidence_accepts_the_fixture(self):
        document = _fixture_document()
        validate_save_evidence(_save_result(document), document)

    def test_validate_save_evidence_rejects_missing_color_and_apps(self):
        document = _fixture_document(color="")
        with self.assertRaises(RuntimeError):
            validate_save_evidence(_save_result(document), document)
        document = _fixture_document(
            applications=_fixture_document()["applications"][:2]
        )
        with self.assertRaises(RuntimeError):
            validate_save_evidence(_save_result(document), document)

    def test_validate_reopen_evidence_accepts_full_proof(self):
        document = _fixture_document(
            applications=[
                {"id": "slot-1", "label": "A", "desktopEntryId": TERMINAL_APP_ID, "urls": []},
                {"id": "slot-2", "label": "B", "desktopEntryId": TERMINAL_APP_ID, "urls": []},
                {"id": "slot-3", "label": "E", "desktopEntryId": EDITOR_APP_ID, "urls": []},
            ]
        )
        validate_reopen_evidence(
            _reopen_result(document), _save_result(document), document
        )

    def test_validate_reopen_evidence_rejects_identity_and_layout_gaps(self):
        document = _fixture_document()
        updated = _fixture_document(
            applications=[
                {"id": "slot-1", "label": "A", "desktopEntryId": TERMINAL_APP_ID, "urls": []},
                {"id": "slot-2", "label": "B", "desktopEntryId": TERMINAL_APP_ID, "urls": []},
                {"id": "slot-3", "label": "E", "desktopEntryId": EDITOR_APP_ID, "urls": []},
            ]
        )
        with self.assertRaises(RuntimeError):
            validate_reopen_evidence(
                _reopen_result(updated, workspaceId="other"),
                _save_result(updated),
                updated,
            )
        with self.assertRaises(RuntimeError):
            validate_reopen_evidence(
                _reopen_result(updated, layoutMatchesSavedDocument=False),
                _save_result(updated),
                updated,
            )
        with self.assertRaises(RuntimeError):
            validate_reopen_evidence(
                _reopen_result(updated), _save_result(updated), document
            )

    def test_nested_environment_variables_rejects_inherited_display(self):
        import os

        environment = {"XDG_DATA_HOME": "/tmp/x"}
        self.assertIs(nested_environment_variables(environment), environment)
        display = os.environ.get("DISPLAY")
        if display:
            with self.assertRaises(RuntimeError):
                nested_environment_variables({"DISPLAY": display})


if __name__ == "__main__":
    unittest.main()
