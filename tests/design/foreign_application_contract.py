#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""ADR-0352 executable design model, NOT a production authority/transport.

Fixture Evidence is already authenticated by assumption. These tests prove
policy consistency only; ED-20 must separately qualify its OS evidence source.
"""
from dataclasses import dataclass, replace
from enum import Enum
import unittest


class Origin(Enum):
    ANDROID = "android"
    WINDOWS = "windows"


@dataclass(frozen=True)
class Binding:
    session: str
    adapter: str
    runtime: str


@dataclass(frozen=True)
class Registration:
    app: str
    origin: Origin
    revision: int
    recipe_digest: str
    profile: str


@dataclass(frozen=True)
class Evidence:
    binding: Binding
    app: str
    revision: int
    launch: str
    window: int
    window_incarnation: str
    process: tuple[int, int]  # PID/start identity obtained by trusted collector.
    parent: tuple[int, str] | None = None


class Policy:
    """Same-thread policy supplied with admitted producer + live OS facts."""

    def __init__(self, binding, admitted_producer, registrations):
        self.binding = binding
        self.producer = admitted_producer
        self.registrations = {r.app: r for r in registrations}
        self.live_windows = {}
        self.live_launches = {}
        self.associations = {}
        self.snapshot_revision = 0

    def replace_runtime(self, binding, admitted_producer):
        self.binding, self.producer = binding, admitted_producer
        self.live_launches.clear()
        self.associations.clear()
        self.snapshot_revision = 0

    def publish(self, producer, binding, revision, records):
        # Neither a sender-provided UUID nor PID authenticates the producer.
        if producer != self.producer or binding != self.binding:
            return False
        if type(revision) is not int or not self.snapshot_revision < revision < 2**64:
            return False
        if len(records) > 4096:
            return False
        candidate = {}
        for e in records:
            r = self.registrations.get(e.app)
            key = (e.window, e.window_incarnation)
            if (r is None or e.binding != binding or e.revision != r.revision
                    or self.live_launches.get(e.launch) != (e.app, binding)
                    or self.live_windows.get(key) != e.process or key in candidate):
                return False
            candidate[key] = e
        for e in records:
            if e.parent is not None:
                parent = candidate.get(e.parent)
                if parent is None or parent.app != e.app or parent.binding != binding:
                    return False
                seen = {(e.window, e.window_incarnation)}
                while parent is not None:
                    parent_key = (parent.window, parent.window_incarnation)
                    if parent_key in seen:
                        return False
                    seen.add(parent_key)
                    parent = candidate.get(parent.parent)
        self.associations = candidate
        self.snapshot_revision = revision
        return True

    def origin(self, window, incarnation, untrusted_hint=""):
        # Display hints never enter the association admission algorithm.
        e = self.associations.get((window, incarnation))
        if e is None or self.live_windows.get((window, incarnation)) != e.process:
            return None
        r = self.registrations.get(e.app)
        if r is None or e.revision != r.revision or e.binding != self.binding:
            return None
        return r.origin

    def may_force_stop(self, app, expected_revision, binding, scope_owned,
                       independently_stoppable):
        r = self.registrations.get(app)
        if (r is None or r.revision != expected_revision
                or binding != self.binding or not scope_owned
                or not independently_stoppable):
            return False
        # Recipe/profile sharing is checked across registrations, not only
        # currently visible windows: a peer may have no top-level window.
        return all(other.app == app or other.profile != r.profile
                   for other in self.registrations.values())


class ContractTests(unittest.TestCase):
    def setUp(self):
        self.b = Binding("session-1", "adapter-1", "runtime-1")
        self.a = Registration("app-a", Origin.WINDOWS, 1, "sha256-a", "prefix-a")
        self.c = Registration("app-b", Origin.WINDOWS, 1, "sha256-b", "prefix-b")
        self.p = Policy(self.b, "inherited-peer-channel", [self.a, self.c])
        self.e = Evidence(self.b, "app-a", 1, "launch-a", 10, "window-1", (20, 30))
        self.f = Evidence(self.b, "app-b", 1, "launch-b", 11, "window-2", (21, 31))
        self.p.live_windows = {(10, "window-1"): (20, 30), (11, "window-2"): (21, 31)}
        self.p.live_launches = {"launch-a": ("app-a", self.b), "launch-b": ("app-b", self.b)}

    def publish(self, records=None, revision=1, producer="inherited-peer-channel"):
        return self.p.publish(producer, self.b, revision, records or [self.e, self.f])

    def test_two_independent_apps_and_independent_stop(self):
        self.assertTrue(self.publish())
        self.assertEqual(self.p.origin(10, "window-1"), Origin.WINDOWS)
        self.assertTrue(self.p.may_force_stop("app-a", 1, self.b, True, True))
        del self.p.live_windows[(10, "window-1")]
        self.assertIsNone(self.p.origin(10, "window-1"))
        self.assertEqual(self.p.origin(11, "window-2"), Origin.WINDOWS)

    def test_spoofed_hint_never_admits_origin(self):
        self.assertIsNone(self.p.origin(10, "window-1", "app-a.desktop"))

    def test_unadmitted_same_user_producer_rejected(self):
        self.assertFalse(self.publish(producer="same-uid-other-channel"))

    def test_runtime_restart_discards_old_associations_and_launches(self):
        self.assertTrue(self.publish())
        self.p.replace_runtime(replace(self.b, runtime="runtime-2"), self.p.producer)
        self.assertIsNone(self.p.origin(10, "window-1"))
        self.assertFalse(self.publish(revision=2))

    def test_session_or_adapter_replacement_rejects_old_snapshot(self):
        for field in ("session", "adapter"):
            self.p.replace_runtime(replace(self.b, **{field: "new"}), self.p.producer)
            self.assertFalse(self.publish())

    def test_re_registration_does_not_adopt_old_window(self):
        self.assertTrue(self.publish())
        self.p.registrations["app-a"] = replace(self.a, revision=2)
        self.assertIsNone(self.p.origin(10, "window-1"))
        self.assertFalse(self.publish(revision=2))

    def test_window_id_reuse_and_pid_reuse_fail_closed(self):
        self.assertTrue(self.publish())
        self.assertIsNone(self.p.origin(10, "new-window"))
        self.p.live_windows[(10, "window-1")] = (20, 99)
        self.assertIsNone(self.p.origin(10, "window-1"))
        self.assertFalse(self.publish(revision=2))

    def test_shared_prefix_even_invisible_peer_blocks_force_stop(self):
        self.p.registrations["app-b"] = replace(self.c, profile=self.a.profile)
        self.assertFalse(self.p.may_force_stop("app-a", 1, self.b, True, True))

    def test_unqualified_stop_and_stale_revision_refused(self):
        for rev, owned, separate in ((2, True, True), (1, False, True), (1, True, False)):
            self.assertFalse(self.p.may_force_stop("app-a", rev, self.b, owned, separate))

    def test_snapshot_revision_replay_and_overflow_refused(self):
        self.assertTrue(self.publish())
        for rev in (1, 0, -1, 2**64, True):
            self.assertFalse(self.publish(revision=rev))

    def test_snapshot_is_atomic_on_invalid_member(self):
        self.assertTrue(self.publish())
        before = dict(self.p.associations)
        self.assertFalse(self.publish([self.e, replace(self.f, app="missing")], 2))
        self.assertEqual(self.p.associations, before)

    def test_duplicate_and_oversize_snapshot_refused(self):
        self.assertFalse(self.publish([self.e, self.e]))
        self.assertFalse(self.publish([self.e] * 4097))

    def test_child_window_cannot_inherit_peer_app_identity(self):
        self.assertFalse(self.publish([self.e, replace(self.f, parent=(10, "window-1"))]))
        child = replace(self.f, app="app-a", launch="launch-a", parent=(10, "window-1"))
        self.assertTrue(self.publish([self.e, child]))

    def test_child_cycle_refused(self):
        child = replace(self.f, app="app-a", launch="launch-a", parent=(10, "window-1"))
        self.assertFalse(self.publish([replace(self.e, parent=(11, "window-2")), child]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
