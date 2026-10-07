#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded ED05 design proof only: no production filesystem/durability evidence."""
from copy import deepcopy
from dataclasses import dataclass, field
import unittest


@dataclass
class Tree:
    identity: str
    # Named entries preserve identity and current bytes through whole-tree rename.
    entries: dict[str, tuple[str, bytes]] = field(default_factory=dict)


@dataclass
class World:
    names: dict[str, Tree] = field(default_factory=dict)
    mount_generation: int = 1
    durable_phase: str = "none"
    events: list[str] = field(default_factory=list)


class Move:
    def __init__(self, world):
        self.world = world
        self.source = world.names["source"]
        self.snapshot = deepcopy(self.source.entries)
        self.mount = world.mount_generation
        self.stage = None
        self.phase = "admitted"
        self.status = "pending"
        self.live = True
        self.retirement_authorized = False
        self.restore_admission = None

    def current(self):
        return self.live and self.mount == self.world.mount_generation

    def prepare(self):
        if (not self.current() or self.phase != "admitted"
                or "recovery" in self.world.names):
            self.status = "refused"
            return False
        self.phase = self.world.durable_phase = "prepared"
        return True

    def copy(self):
        if not self.current() or self.phase != "prepared":
            return False
        if "stage" in self.world.names:
            self.status = "collision"
            return False
        self.stage = Tree("owned-stage", deepcopy(self.snapshot))
        self.world.names["stage"] = self.stage
        self.phase = "copying"
        return True

    def cleanup_stage(self):
        # Failed O_EXCL never establishes ownership. Neither does a path string.
        if (self.current() and self.phase in ("prepared", "copying", "verified")
                and self.stage is not None and self.world.names.get("stage") is self.stage
                and self.stage.entries == self.snapshot):
            del self.world.names["stage"]
            self.world.events.append("remove-owned-stage")

    def verify(self):
        if not self.current() or self.phase != "copying":
            return False
        if (self.world.names.get("source") is not self.source
                or self.source.entries != self.snapshot
                or self.world.names.get("stage") is not self.stage
                or self.stage.entries != self.snapshot):
            self.status = "changed"
            return False
        self.phase = self.world.durable_phase = "verified"
        return True

    def destination_current(self):
        return (self.current() and self.stage is not None
                and self.world.names.get("destination") is self.stage
                and self.stage.entries == self.snapshot)

    def publish(self, barrier=True, before_rename=None):
        if not self.current() or self.phase != "verified":
            return False
        if (self.world.names.get("stage") is not self.stage
                or self.stage.entries != self.snapshot):
            self.status = "changed-stage"
            return False
        if before_rename:
            before_rename()  # Models the syscall window, not production authority.
        if "destination" in self.world.names:
            self.status = "collision"
            return False
        if "stage" not in self.world.names:
            self.status = "vanished-stage"
            return False
        self.world.names["destination"] = self.world.names.pop("stage")
        self.world.events.append("publish")
        if not self.destination_current():
            self.status = "changed-publication"
            self.phase = "publication-observed"
            return False
        if not barrier:
            self.status = "uncertain-publication"
            self.phase = "publication-observed"
            return False
        self.phase = self.world.durable_phase = "published"
        return True

    def begin_retire(self):
        if not self.current() or self.phase != "published":
            return False
        if not self.destination_current():
            self.status = "changed-destination"
            return False
        if (self.world.names.get("source") is not self.source
                or self.source.entries != self.snapshot):
            self.status = "source-changed"
            return False
        self.world.durable_phase = "retirement-intent"
        self.retirement_authorized = True
        return True

    def retire(self, barrier=True, block_restore=False, after_rename=None):
        if not self.current() or not self.retirement_authorized:
            return False
        self.retirement_authorized = False
        if not self.destination_current():
            self.status = "changed-destination"
            return False
        if "recovery" in self.world.names or "source" not in self.world.names:
            self.status = "retirement-conflict"
            return False
        captured = self.world.names.pop("source")
        self.world.names["recovery"] = captured
        self.world.events.append("retain-source-tree")
        if after_rename:
            after_rename()
        # Placement and captured identity are independent of destination truth.
        # A destination error must not manufacture evidence about the source.
        if self.world.names.get("recovery") is not captured:
            self.status = "recovery-placement-unconfirmed"
            return False
        if captured is not self.source:
            restored = (not block_restore and self.current()
                        and "source" not in self.world.names)
            if restored:
                self.world.names["source"] = self.world.names.pop("recovery")
            self.status = ("unexpected-entry-restored-original-unconfirmed" if restored
                           else "unexpected-entry-retained-original-unconfirmed")
            return False
        if not self.destination_current():
            self.status = "destination-changed-source-retained"
            return False
        if "source" in self.world.names:
            self.status = "source-name-recreated-original-retained"
            return False
        if not barrier:
            self.status = "uncertain-retirement"
            return False
        self.phase = self.world.durable_phase = "retained"
        self.status = ("completed-with-retention" if captured.entries == self.snapshot
                       else "changed-source-retained")
        return self.status == "completed-with-retention"

    def cancel(self):
        self.retirement_authorized = False
        self.status = ("destination-complete-source-retained"
                       if self.destination_current() else "cancelled-inspection-required")
        self.live = False

    def restart(self):
        self.live = False
        self.retirement_authorized = False
        self.status = "inspection-required"
        self.restore_admission = None
        # Persisted phase is not authority to replay any filesystem operation.

    def admit_restore(self, deliberate=False):
        self.restore_admission = None
        payload = self.world.names.get("recovery")
        if deliberate and payload is self.source:
            self.restore_admission = (payload, self.world.mount_generation)
            return True
        return False

    def restore(self):
        admitted = self.restore_admission
        self.restore_admission = None
        if (admitted is None
                or self.world.names.get("recovery") is not admitted[0]
                or self.world.mount_generation != admitted[1]):
            return False
        if "source" in self.world.names:
            self.status = "restore-collision"
            return False
        self.world.names["source"] = self.world.names.pop("recovery")
        self.world.events.append("deliberate-restore")
        self.status = "restored"
        return True


def fixture():
    w = World({"source": Tree("source-inode", {"child": ("child-inode", b"original")})})
    return w, Move(w)


def through(move, phase):
    for name in ("prepare", "copy", "verify", "publish", "begin_retire", "retire"):
        result = getattr(move, name)()
        if not result:
            raise AssertionError((name, move.status))
        if name == phase:
            break


class PreservationContract(unittest.TestCase):
    def test_wrong_captured_entry_never_overwrites_recreated_source_name(self):
        w, m = fixture()
        through(m, "begin_retire")
        foreign = Tree("foreign-source")
        replacement = Tree("new-original-name")
        w.names["source"] = foreign
        def recreate():
            w.names["source"] = replacement
        self.assertFalse(m.retire(after_rename=recreate))
        self.assertIs(w.names["source"], replacement)
        self.assertIs(w.names["recovery"], foreign)
        self.assertEqual(m.status, "unexpected-entry-retained-original-unconfirmed")

    def test_recovery_replacement_cannot_claim_original_retained(self):
        w, m = fixture()
        through(m, "begin_retire")
        foreign = Tree("foreign-recovery")
        def replace():
            w.names["recovery"] = foreign
        self.assertFalse(m.retire(after_rename=replace))
        self.assertEqual(m.status, "recovery-placement-unconfirmed")
        self.assertIs(w.names["recovery"], foreign)

    def test_wrong_captured_entry_is_not_restored_after_mount_loss(self):
        w, m = fixture()
        through(m, "begin_retire")
        foreign = Tree("foreign-source")
        w.names["source"] = foreign
        def replace():
            w.mount_generation += 1
        self.assertFalse(m.retire(after_rename=replace))
        self.assertIs(w.names["recovery"], foreign)
        self.assertNotIn("source", w.names)

    def test_reviewer_mount_loss_preserves_even_owned_partial_stage(self):
        w, m = fixture()
        through(m, "copy")
        w.mount_generation += 1
        before = deepcopy(w)
        m.cleanup_stage()
        self.assertEqual(w, before)

    def test_reviewer_combined_replacements_cannot_claim_original_retained(self):
        w, m = fixture()
        through(m, "begin_retire")
        foreign = Tree("foreign-source")
        w.names["source"] = foreign
        def replace_destination():
            w.names["destination"] = Tree("foreign-destination")
        self.assertFalse(m.retire(after_rename=replace_destination, block_restore=True))
        self.assertIs(w.names["recovery"], foreign)
        self.assertNotIn("source-retained", m.status)
        self.assertEqual(m.status, "unexpected-entry-retained-original-unconfirmed")

    def test_stage_replacement_in_publish_syscall_window_preserves_foreign(self):
        w, m = fixture()
        through(m, "verify")
        foreign = Tree("racing-stage", {"keep": ("foreign", b"keep")})
        def replace():
            w.names["stage"] = foreign
        self.assertFalse(m.publish(before_rename=replace))
        self.assertEqual(m.status, "changed-publication")
        self.assertFalse(m.begin_retire())
        self.assertIs(w.names["source"], m.source)
        self.assertIs(w.names["destination"], foreign)
        m.cleanup_stage()
        self.assertIs(w.names["destination"], foreign)

    def test_destination_write_before_retire_requires_inspection(self):
        w, m = fixture()
        through(m, "begin_retire")
        w.names["destination"].entries["child"] = ("child-inode", b"new")
        self.assertFalse(m.retire())
        self.assertIs(w.names["source"], m.source)
        self.assertEqual(w.names["destination"].entries["child"][1], b"new")

    def test_destination_replacement_after_source_rename_cannot_complete(self):
        w, m = fixture()
        through(m, "begin_retire")
        foreign = Tree("racing-destination")
        def replace():
            w.names["destination"] = foreign
        self.assertFalse(m.retire(after_rename=replace))
        self.assertEqual(m.status, "destination-changed-source-retained")
        self.assertIs(w.names["recovery"], m.source)
        self.assertIs(w.names["destination"], foreign)

    def test_mount_replacement_after_source_rename_cannot_complete(self):
        w, m = fixture()
        through(m, "begin_retire")
        def replace():
            w.mount_generation += 1
        self.assertFalse(m.retire(after_rename=replace))
        self.assertIs(w.names["recovery"], m.source)
        self.assertNotEqual(m.status, "completed-with-retention")

    def test_reviewer_stage_replacement_cannot_become_completed_move(self):
        w, m = fixture()
        through(m, "verify")
        foreign = Tree("foreign-replacement", {"keep": ("foreign", b"keep")})
        w.names["stage"] = foreign
        self.assertFalse(m.publish())
        self.assertFalse(m.begin_retire())
        self.assertFalse(m.retire())
        self.assertIs(w.names["source"], m.source)
        self.assertTrue(any(tree is foreign for tree in w.names.values()))

    def test_reviewer_destination_replacement_cannot_become_completed_move(self):
        w, m = fixture()
        through(m, "publish")
        foreign = Tree("foreign-destination", {"keep": ("foreign", b"keep")})
        w.names["destination"] = foreign
        self.assertFalse(m.begin_retire())
        self.assertFalse(m.retire())
        self.assertIs(w.names["source"], m.source)
        self.assertIs(w.names["destination"], foreign)

    def test_restart_cannot_cleanup_even_previously_owned_stage(self):
        w, m = fixture()
        through(m, "copy")
        before = deepcopy(w)
        m.restart()
        m.cleanup_stage()
        self.assertEqual(w, before)

    def test_foreign_stage_collision_cannot_grant_cleanup(self):
        w, m = fixture()
        foreign = Tree("foreign", {"keep": ("other-inode", b"unrelated")})
        w.names["stage"] = foreign
        m.prepare()
        self.assertFalse(m.copy())
        m.cleanup_stage()
        self.assertIs(w.names["stage"], foreign)
        self.assertEqual(foreign.entries["keep"][1], b"unrelated")

    def test_replaced_owned_stage_survives_cleanup(self):
        w, m = fixture()
        through(m, "copy")
        original = w.names.pop("stage")
        foreign = Tree("replacement")
        w.names["stage"] = foreign
        m.cleanup_stage()
        self.assertIs(w.names["stage"], foreign)
        self.assertEqual(original.entries, m.snapshot)

    def test_foreign_descendant_blocks_recursive_cleanup(self):
        w, m = fixture()
        through(m, "copy")
        w.names["stage"].entries["foreign"] = ("foreign-inode", b"keep")
        m.cleanup_stage()
        self.assertEqual(w.names["stage"].entries["foreign"][1], b"keep")

    def test_owned_unpublished_stage_can_be_removed(self):
        w, m = fixture()
        through(m, "copy")
        m.cleanup_stage()
        self.assertNotIn("stage", w.names)
        self.assertIs(w.names["source"], m.source)

    def test_existing_destination_survives_no_replace_publication(self):
        w, m = fixture()
        through(m, "verify")
        foreign = Tree("foreign-destination")
        w.names["destination"] = foreign
        self.assertFalse(m.publish())
        m.cleanup_stage()
        self.assertIs(w.names["destination"], foreign)
        self.assertIs(w.names["source"], m.source)

    def test_readback_corruption_blocks_source_retirement(self):
        w, m = fixture()
        through(m, "copy")
        w.names["stage"].entries["child"] = ("child-inode", b"corrupt")
        self.assertFalse(m.verify())
        self.assertFalse(m.begin_retire())
        self.assertIs(w.names["source"], m.source)

    def test_child_write_after_copy_is_not_hidden_by_parent_identity(self):
        w, m = fixture()
        through(m, "copy")
        m.source.entries["child"] = ("child-inode", b"late")
        self.assertFalse(m.verify())
        self.assertEqual(w.names["source"].entries["child"][1], b"late")

    def test_child_write_after_publish_preserves_both_snapshots(self):
        w, m = fixture()
        through(m, "publish")
        m.source.entries["child"] = ("child-inode", b"late")
        self.assertFalse(m.begin_retire())
        self.assertEqual(w.names["source"].entries["child"][1], b"late")
        self.assertEqual(w.names["destination"].entries["child"][1], b"original")

    def test_child_write_in_retirement_race_is_retained(self):
        w, m = fixture()
        through(m, "begin_retire")
        m.source.entries["child"] = ("child-inode", b"late")
        self.assertFalse(m.retire())
        self.assertEqual(m.status, "changed-source-retained")
        self.assertEqual(w.names["recovery"].entries["child"][1], b"late")

    def test_new_descendant_in_retirement_race_is_retained(self):
        w, m = fixture()
        through(m, "begin_retire")
        m.source.entries["new"] = ("new-inode", b"new bytes")
        self.assertFalse(m.retire())
        self.assertIn("new", w.names["recovery"].entries)

    def test_open_named_inode_after_retirement_keeps_late_bytes(self):
        w, m = fixture()
        inode_tree = m.source
        through(m, "retire")
        inode_tree.entries["child"] = ("child-inode", b"after move")
        self.assertEqual(w.names["recovery"].entries["child"][1], b"after move")
        self.assertEqual(w.names["destination"].entries["child"][1], b"original")

    def test_source_replacement_after_check_is_restored_without_delete(self):
        w, m = fixture()
        through(m, "begin_retire")
        foreign = Tree("replacement")
        w.names["source"] = foreign
        self.assertFalse(m.retire())
        self.assertIs(w.names["source"], foreign)
        self.assertIn("destination", w.names)

    def test_failed_wrong_identity_restoration_retains_captured_entry(self):
        w, m = fixture()
        through(m, "begin_retire")
        foreign = Tree("replacement")
        w.names["source"] = foreign
        self.assertFalse(m.retire(block_restore=True))
        self.assertIs(w.names["recovery"], foreign)
        self.assertEqual(m.status, "unexpected-entry-retained-original-unconfirmed")

    def test_mount_replacement_cannot_reuse_live_retirement_authority(self):
        w, m = fixture()
        through(m, "begin_retire")
        w.mount_generation += 1
        self.assertFalse(m.retire())
        self.assertIs(w.names["source"], m.source)

    def test_publication_durability_failure_has_no_retirement_permission(self):
        w, m = fixture()
        through(m, "verify")
        self.assertFalse(m.publish(barrier=False))
        self.assertFalse(m.begin_retire())
        self.assertIn("destination", w.names)
        self.assertIs(w.names["source"], m.source)

    def test_retirement_durability_failure_preserves_payload(self):
        w, m = fixture()
        through(m, "begin_retire")
        self.assertFalse(m.retire(barrier=False))
        self.assertIs(w.names["recovery"], m.source)
        self.assertIn("destination", w.names)
        self.assertEqual(m.status, "uncertain-retirement")

    def test_cancellation_at_every_boundary_preserves_named_source_bytes(self):
        for phase in ("prepare", "copy", "verify", "publish", "begin_retire", "retire"):
            with self.subTest(phase=phase):
                w, m = fixture()
                through(m, phase)
                m.cancel()
                self.assertFalse(m.retire())
                self.assertTrue(any(tree is m.source for tree in w.names.values()))
                self.assertEqual(m.source.entries["child"][1], b"original")
                if "destination" in w.names:
                    m.cleanup_stage()
                    self.assertIn("destination", w.names)

    def test_restart_at_every_boundary_never_replays_effects(self):
        for phase in ("prepare", "copy", "verify", "publish", "begin_retire", "retire"):
            with self.subTest(phase=phase):
                w, m = fixture()
                through(m, phase)
                before = deepcopy(w)
                m.restart()
                self.assertFalse(m.publish())
                self.assertFalse(m.begin_retire())
                self.assertFalse(m.retire())
                self.assertFalse(m.restore())
                self.assertEqual(w, before)

    def test_fresh_restore_admission_fences_mount_and_payload_replacement(self):
        for replace_mount in (True, False):
            with self.subTest(replace_mount=replace_mount):
                w, m = fixture()
                through(m, "retire")
                m.restart()
                self.assertTrue(m.admit_restore(deliberate=True))
                if replace_mount:
                    w.mount_generation += 1
                else:
                    w.names["recovery"] = Tree("foreign")
                before = deepcopy(w)
                self.assertFalse(m.restore())
                self.assertEqual(w, before)

    def test_restore_collision_keeps_recovery_and_destination(self):
        w, m = fixture()
        through(m, "retire")
        foreign = Tree("new-original-name")
        w.names["source"] = foreign
        self.assertTrue(m.admit_restore(deliberate=True))
        self.assertFalse(m.restore())
        self.assertIs(w.names["source"], foreign)
        self.assertIs(w.names["recovery"], m.source)
        self.assertIn("destination", w.names)

    def test_explicit_restore_returns_current_retained_bytes(self):
        w, m = fixture()
        through(m, "retire")
        m.source.entries["child"] = ("child-inode", b"late retained write")
        m.restart()
        self.assertTrue(m.admit_restore(deliberate=True))
        self.assertTrue(m.restore())
        self.assertEqual(w.names["source"].entries["child"][1], b"late retained write")
        self.assertIn("destination", w.names)


if __name__ == "__main__":
    unittest.main(verbosity=2)
