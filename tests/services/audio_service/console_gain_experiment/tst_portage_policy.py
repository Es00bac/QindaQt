#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Process-only feature failures; no Portage import/build or native runtime."""
import unittest
from portage_policy import cohort_features


class CohortFeatures(unittest.TestCase):
    def setUp(self):
        self.baseline = {"sandbox", "usersandbox", "network-sandbox", "pid-sandbox",
                         "ipc-sandbox", "userpriv", "strict", "binpkg-signing",
                         "splitdebug", "compressdebug", "ccache", "distcc", "fail-clean",
                         "merge-wait", "noauto", "preserve-libs"}

    def test_normal_retains_signing_and_every_sandbox(self):
        result = cohort_features(self.baseline, asan=False, upstream_tests=False)
        self.assertTrue({"sandbox", "usersandbox", "network-sandbox", "pid-sandbox",
                         "ipc-sandbox", "userpriv", "strict", "binpkg-signing",
                         "splitdebug", "compressdebug"}.issubset(result))
        self.assertNotIn("test", result)
        self.assertNotIn("nostrip", result)

    def test_private_asan_removes_only_known_incompatible_features(self):
        before = self.baseline.copy()
        result = cohort_features(self.baseline, asan=True, upstream_tests=False)
        self.assertEqual(before - result, {"sandbox", "usersandbox", "binpkg-signing",
                         "splitdebug", "compressdebug", "ccache", "distcc",
                         "fail-clean", "merge-wait", "noauto"})
        self.assertTrue({"network-sandbox", "pid-sandbox", "ipc-sandbox",
                         "userpriv", "strict", "preserve-libs", "test", "nostrip"}
                        .issubset(result))
        self.assertEqual(self.baseline, before)

    def test_normal_test_variant_keeps_normal_authority(self):
        result = cohort_features(self.baseline, asan=False, upstream_tests=True)
        self.assertTrue({"test", "binpkg-signing", "sandbox", "usersandbox"}.issubset(result))

    def test_unsigned_normal_refuses_instead_of_silently_weakening(self):
        with self.assertRaises(ValueError):
            cohort_features(self.baseline - {"binpkg-signing"}, asan=False,
                            upstream_tests=False)

    def test_private_asan_can_use_unsigned_diagnostic_package(self):
        result = cohort_features(self.baseline - {"binpkg-signing"}, asan=True,
                                 upstream_tests=False)
        self.assertIn("nostrip", result)
        self.assertNotIn("binpkg-signing", result)

    def test_test_fail_continue_is_never_admitted(self):
        for asan in (True, False):
            with self.assertRaises(ValueError):
                cohort_features(self.baseline | {"test-fail-continue"}, asan=asan, upstream_tests=True)

    def test_retention_policy_does_not_mutate_callers_baseline(self):
        before = self.baseline.copy()
        result = cohort_features(self.baseline, asan=False, upstream_tests=False)
        self.assertTrue({"keepwork", "keeptemp", "noclean"}.issubset(result))
        self.assertFalse({"ccache", "distcc", "fail-clean", "merge-wait", "noauto"} & result)
        self.assertEqual(self.baseline, before)


if __name__ == "__main__":
    unittest.main()
