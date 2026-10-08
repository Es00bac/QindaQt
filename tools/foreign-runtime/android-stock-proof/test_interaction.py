"""Injected collaborators only: never a guest, bus, process or input device."""
import copy
import unittest
from unittest.mock import patch
import frame_capture
import interaction


def row(active=True, stack=0):
    return {"geometry": {"x": 0, "y": 0, "width": 1280, "height": 800},
            "active": active, "stackIndex": stack}


class InteractionTests(unittest.TestCase):
    def test_other_window_loss_is_refused(self):
        with self.assertRaisesRegex(RuntimeError, "unrelated-window"):
            interaction.unchanged_others({"a": row(), "b": row()}, {"a": row()}, "a")

    def test_other_window_geometry_change_is_refused(self):
        before = {"a": row(), "b": row()}
        after = copy.deepcopy(before); after["b"]["geometry"]["width"] = 900
        with self.assertRaisesRegex(RuntimeError, "unrelated-window-moved"):
            interaction.unchanged_others(before, after, "a")

    def test_only_target_geometry_can_change(self):
        before = {"a": row(), "b": row()}
        after = copy.deepcopy(before); after["a"]["geometry"]["width"] = 900
        interaction.unchanged_others(before, after, "a")

    def test_owner_loss_precedes_capture_access(self):
        with patch.object(frame_capture, "process_version") as version:
            with self.assertRaisesRegex(RuntimeError, "owner-lost"):
                frame_capture.capture(42, (), "x",
                    lambda: (_ for _ in ()).throw(RuntimeError("owner-lost")))
            version.assert_not_called()

    def test_replaced_compositor_is_refused_before_fd_access(self):
        with patch.object(frame_capture, "process_version", return_value=("new",)):
            with self.assertRaisesRegex(RuntimeError, "compositor-replaced"):
                frame_capture.capture(42, ("old",), "x", lambda: None)

    def test_png_rejects_wrong_size(self):
        with self.assertRaisesRegex(RuntimeError, "frame-format"):
            frame_capture.png(b"short")

    def test_png_has_exact_fixed_dimensions(self):
        pixels = b"\x11\x22\x33\xff" * (1280 * 800)
        encoded = frame_capture.png(pixels)
        self.assertEqual(encoded[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(encoded[16:24], b"\0\0\x05\0\0\0\x03\x20")

    def test_extra_window_refuses_before_input(self):
        rows = {"calc": row(False), "clock": row(True, 1), "foreign": row()}
        with patch.object(interaction, "process_version", return_value=("pinned",)):
            with self.assertRaisesRegex(RuntimeError, "initial-window-set"):
                interaction.run(lambda: rows, lambda _: self.fail("input dispatched"),
                    lambda: None, lambda _: self.fail("launch dispatched"), 42,
                    ["calc", "clock"], lambda: 100)

    def test_inactive_clock_refuses_before_input(self):
        rows = {"calc": row(True), "clock": row(False, 1)}
        with patch.object(interaction, "process_version", return_value=("pinned",)):
            with self.assertRaisesRegex(RuntimeError, "initial-window-set"):
                interaction.run(lambda: rows, lambda _: self.fail("input dispatched"),
                    lambda: None, lambda _: None, 42, ["calc", "clock"], lambda: 100)

    def test_full_injected_sequence_retains_each_other_app(self):
        rows = {"calc": row(False), "clock": row(True, 1)}
        keys = set(); pointer = [0, 0]; resizing = [None]; closing = [None]
        removals = []; launches = []; captures = []
        def target():
            x, y = pointer
            candidates = [(r["stackIndex"], key) for key, r in rows.items()
                          if r["geometry"]["x"] <= x < r["geometry"]["width"]
                          and r["geometry"]["y"] <= y < r["geometry"]["height"]]
            return max(candidates)[1]
        def inject(events):
            for event in events:
                if event["type"] == "pointer-absolute":
                    pointer[:] = [event["x"], event["y"]]
                    if resizing[0]:
                        rows[resizing[0]]["geometry"].update(
                            width=1080 if resizing[0] == "calc" else 980,
                            height=680 if resizing[0] == "calc" else 580)
                elif event["type"] == "key":
                    if event["pressed"]:
                        keys.add(event["key"])
                        if event["key"] == "enter" and closing[0]:
                            doomed = closing[0]; del rows[doomed]
                            removals.append((doomed, sorted(rows))); closing[0] = None
                            for row_value in rows.values(): row_value["active"] = True
                    else: keys.discard(event["key"])
                elif event["type"] == "button":
                    if event["pressed"] and "left-meta" in keys: resizing[0] = target()
                    elif event["pressed"] and "left-alt" in keys: closing[0] = target()
                    elif not event["pressed"]: resizing[0] = None
        def launch(index):
            name = "new-clock" if index == 1 else "new-calc"
            launches.append(index)
            for row_value in rows.values(): row_value["active"] = False
            rows[name] = row(True, 2 + len(launches))
        def capture(*args):
            captures.append(args[2])
            return {"label": args[2]}, bytes([len(captures)])
        with patch.object(interaction, "process_version", return_value=("pinned",)), patch.object(
                interaction, "capture", side_effect=capture), patch.object(interaction.time, "sleep"):
            result = interaction.run(lambda: copy.deepcopy(rows), inject, lambda: None,
                                     launch, 42, ["calc", "clock"], lambda: 100)
        self.assertEqual(removals, [("clock", ["calc"]), ("calc", ["new-clock"])])
        self.assertEqual(launches, [1, 0])
        self.assertEqual(set(result["finalWindowIds"]), {"new-calc", "new-clock"})
        self.assertFalse(result["identityAuthority"])
        self.assertEqual(keys, set())
        self.assertLessEqual(len(captures), 8)


if __name__ == "__main__":
    unittest.main()
