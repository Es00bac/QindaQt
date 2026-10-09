# SPDX-License-Identifier: GPL-3.0-or-later
"""Private build-only feature policy; never edits Portage configuration."""
from __future__ import annotations


def cohort_features(baseline: set[str], *, asan: bool, upstream_tests: bool) -> set[str]:
    """Return a new process-only policy; normal signing remains mandatory."""
    if "test-fail-continue" in baseline:
        raise ValueError("cohort forbids test-fail-continue")
    features = baseline - {"ccache", "distcc", "fail-clean", "merge-wait", "noauto"}
    features.update({"keepwork", "keeptemp", "noclean"})
    if asan or upstream_tests:
        features.add("test")
    if asan:
        # AGENT-GUARD: Gentoo's LD_PRELOAD sandbox precedes libasan and aborts
        # Meson's sanity binary. Only these private ASan cohorts remove it;
        # do not suppress ASan checks or weaken normal package isolation.
        # https://wiki.gentoo.org/wiki/AddressSanitizer#Per-package
        features.difference_update({"sandbox", "usersandbox", "binpkg-signing",
                                    "splitdebug", "compressdebug"})
        features.add("nostrip")
    elif "binpkg-signing" not in features:
        raise ValueError("normal package baseline must already have Portage binpkg-signing")
    return features
