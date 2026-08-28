# SPDX-License-Identifier: GPL-3.0-or-later
"""Hostile deadline and archive tests for desktop readiness probing."""

from __future__ import annotations

import json
import unittest
from io import StringIO

from desktop_session_readiness import (
    MARKER,
    PROBE_LIFETIME_SECONDS,
    ReadinessDeadlineExpired,
    await_complete_snapshot,
    parse_and_archive_probe,
    require_probe_lifetime,
)
from test_desktop_session_topology_unit import ready_probe


class ReadinessProbeTests(unittest.TestCase):
    def test_near_outer_deadline_never_shrinks_probe_lifetime(self) -> None:
        self.assertEqual(
            require_probe_lifetime(PROBE_LIFETIME_SECONDS),
            PROBE_LIFETIME_SECONDS,
        )
        with self.assertRaisesRegex(
            ReadinessDeadlineExpired, "no complete probe lifetime"
        ):
            require_probe_lifetime(PROBE_LIFETIME_SECONDS - 0.001)

    def test_budget_expiry_reports_last_failed_observation(self) -> None:
        pending = ready_probe()
        pending["windows"]["windows"] = []  # type: ignore[index]
        calls = 0

        def sample(_: float) -> dict[str, object]:
            nonlocal calls
            calls += 1
            if calls == 1:
                return pending
            raise ReadinessDeadlineExpired("no complete probe lifetime remains")

        with self.assertRaisesRegex(
            RuntimeError,
            "mapped test application was missing: org.qindaqt.Settings; "
            "no complete probe lifetime remains",
        ):
            await_complete_snapshot(sample, seconds=2, sleep=lambda _: None)
        self.assertEqual(calls, 2)

    def test_observation_is_archived_before_schema_rejection(self) -> None:
        line = MARKER + json.dumps({"schemaVersion": 2}) + "\n"
        log = StringIO()
        with self.assertRaisesRegex(RuntimeError, "invalid document"):
            parse_and_archive_probe(line, log)
        self.assertEqual(log.getvalue(), line)


if __name__ == "__main__":
    unittest.main()
