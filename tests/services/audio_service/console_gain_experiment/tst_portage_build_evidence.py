#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exception/evidence orchestration; mocked runner never calls ebuild/Portage."""
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

from portage_recipe import build_attempt


class BuildEvidence(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="audio-build-evidence-")
        self.output = Path(self.temporary.name)
        self.inputs = Mock()
        self.inputs.verify.return_value = {"verified": True, "observed": "fixture"}
        self.record = Mock()
        self.step = {"name": "portage-build-only", "exit": 0, "timedOut": False}

    def tearDown(self):
        self.temporary.cleanup()

    def attempt(self):
        return build_attempt(Path("/fixture/pinned.ebuild"), ["configure"],
                             {"PATH": "/usr/bin:/bin"}, self.output, self.inputs, self.record)

    def proof(self):
        return json.loads((self.output / "input-final-verification.json").read_text())

    def test_spawn_exception_still_records_final_inputs_and_preserves_original(self):
        original = OSError("owned runner spawn failed")
        with patch("portage_recipe.run_logged", side_effect=original):
            with self.assertRaises(OSError) as caught:
                self.attempt()
        self.assertIs(caught.exception, original)
        self.inputs.verify.assert_called_once()
        self.record.assert_not_called()
        self.assertTrue(self.proof()["verified"])

    def test_record_exception_still_records_final_inputs_and_preserves_original(self):
        original = ValueError("owned result recording failed")
        self.record.side_effect = original
        with patch("portage_recipe.run_logged", return_value=self.step):
            with self.assertRaises(ValueError) as caught:
                self.attempt()
        self.assertIs(caught.exception, original)
        self.inputs.verify.assert_called_once()
        self.assertTrue(self.proof()["verified"])

    def test_secondary_verifier_error_never_hides_build_failure(self):
        original = OSError("first build failure")
        self.inputs.verify.side_effect = ValueError("second input failure")
        stderr = io.StringIO()
        with patch("portage_recipe.run_logged", side_effect=original):
            with contextlib.redirect_stderr(stderr):
                with self.assertRaises(OSError) as caught:
                    self.attempt()
        self.assertIs(caught.exception, original)
        self.assertFalse(self.proof()["verified"])
        self.assertIn("original build exception preserved", stderr.getvalue())

    def test_verifier_error_after_success_refuses_success(self):
        original = ValueError("input mutation after returned success")
        self.inputs.verify.side_effect = original
        with patch("portage_recipe.run_logged", return_value=self.step):
            with self.assertRaises(ValueError) as caught:
                self.attempt()
        self.assertIs(caught.exception, original)
        self.assertFalse(self.proof()["verified"])

    def test_evidence_write_errors_do_not_erase_record_failure(self):
        original = ValueError("first record failure")
        self.record.side_effect = original
        stderr = io.StringIO()
        with patch("portage_recipe.run_logged", return_value=self.step):
            with patch.object(Path, "write_text", side_effect=OSError("unwritable evidence")) as write:
                with contextlib.redirect_stderr(stderr):
                    with self.assertRaises(ValueError) as caught:
                        self.attempt()
        self.assertIs(caught.exception, original)
        self.assertEqual(write.call_count, 2)
        self.assertIn("could not be written", stderr.getvalue())
        self.assertIn("original build exception preserved", stderr.getvalue())

    def test_nonzero_and_timeout_returns_both_keep_input_evidence(self):
        for exitcode, timed_out in ((1, False), (124, True)):
            with self.subTest(exit=exitcode, timeout=timed_out):
                step = {**self.step, "exit": exitcode, "timedOut": timed_out}
                with patch("portage_recipe.run_logged", return_value=step):
                    self.assertIs(self.attempt(), step)
                self.assertTrue(self.proof()["verified"])

    def test_explicit_runner_is_constructor_visible_without_global_override(self):
        runner = Mock(return_value=self.step)
        with patch("portage_recipe.run_logged", side_effect=AssertionError("ambient runner")):
            row = build_attempt(Path("/fixture/pinned.ebuild"), ["configure"],
                {"PATH": "/usr/bin:/bin"}, self.output, self.inputs, self.record, runner=runner)
        self.assertIs(row, self.step)
        self.assertEqual(runner.call_args.args[0], ["/usr/bin/ebuild", "/fixture/pinned.ebuild", "configure"])
        self.assertTrue(self.proof()["verified"])

    def test_injected_cancellation_keeps_final_input_proof_and_original(self):
        from process_evidence import ProcessCancelled
        original = ProcessCancelled("synthetic request")
        with self.assertRaises(ProcessCancelled) as caught:
            build_attempt(Path("/fixture/pinned.ebuild"), ["configure"],
                {"PATH": "/usr/bin:/bin"}, self.output, self.inputs, self.record,
                runner=Mock(side_effect=original))
        self.assertIs(caught.exception, original)
        self.record.assert_not_called()
        self.assertTrue(self.proof()["verified"])


if __name__ == "__main__":
    unittest.main()
