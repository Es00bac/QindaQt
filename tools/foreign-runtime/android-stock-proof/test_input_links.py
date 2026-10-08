import os
from pathlib import Path
import tempfile
import unittest
from input_links import chain, public_name


class InputLinks(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root/"usr/lib64").mkdir(parents=True)
        (self.root/"usr/lib64/end").write_bytes(b"fixed")
        self.owners = {"/usr/lib64/end": [{"kind": "file", "cpv": "fixture/lib-1"}]}

    def tearDown(self):
        self.tmp.cleanup()

    def link(self, name, target):
        (self.root/"usr/lib64"/name).symlink_to(target)
        self.owners["/usr/lib64/"+name] = [{"kind": "symlink", "target": target,
                                          "cpv": "fixture/lib-1"}]

    def trace(self, name="one", limit=40):
        return chain("/usr/lib64/"+name, self.owners, self.root, os.getuid(), limit)

    def test_all_intermediate_hops_and_terminal_preserved(self):
        self.link("one", "two"); self.link("two", "end")
        links, terminal = self.trace()
        self.assertEqual([name for name, _ in links], ["/usr/lib64/one", "/usr/lib64/two"])
        self.assertEqual(terminal, "/usr/lib64/end")
        self.assertTrue(all(row["sourceMode"] == 0o777 and len(row["targetSha256"]) == 64 for _, row in links))

    def test_cycle_refused(self):
        self.link("one", "two"); self.link("two", "one")
        with self.assertRaisesRegex(ValueError, "cycle"): self.trace()

    def test_depth_refused(self):
        self.link("one", "two"); self.link("two", "end")
        with self.assertRaisesRegex(ValueError, "depth"): self.trace(limit=1)

    def test_absolute_and_relative_escape_refused(self):
        for target in ("/etc/passwd", "../../etc/passwd", "/home/user/file"):
            with self.subTest(target=target):
                self.link("one", target)
                try:
                    with self.assertRaisesRegex(ValueError, "outside-public"): self.trace()
                finally: (self.root/"usr/lib64/one").unlink()

    def test_unknown_intermediate_refused(self):
        self.link("one", "two"); self.link("two", "end")
        del self.owners["/usr/lib64/two"]
        with self.assertRaisesRegex(ValueError, "unowned"): self.trace()

    def test_vdb_target_mismatch_refused(self):
        self.link("one", "end")
        self.owners["/usr/lib64/one"][0]["target"] = "other"
        with self.assertRaisesRegex(ValueError, "witness"): self.trace()

    def test_special_terminal_refused(self):
        self.link("one", "end")
        (self.root/"usr/lib64/end").unlink()
        os.mkfifo(self.root/"usr/lib64/end")
        with self.assertRaisesRegex(ValueError, "special"): self.trace()

    def test_fixed_usr_merge_alias_preserves_hops(self):
        self.link("one", "/lib64/end")
        links, terminal = self.trace()
        self.assertEqual(terminal, "/usr/lib64/end")
        self.assertEqual(len(links), 1)
        self.assertEqual(public_name("/sbin/tool"), "/usr/bin/tool")
        self.assertEqual(public_name("//usr/lib64/end"), "/usr/lib64/end")
        with self.assertRaises(ValueError): public_name("usr/lib64/end")


if __name__ == "__main__":
    unittest.main()
