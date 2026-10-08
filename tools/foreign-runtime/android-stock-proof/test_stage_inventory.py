import hashlib
from pathlib import Path
import stat
import tempfile
import unittest
from boot_plan import archive_paths, plan, REQUIRED, KERNEL
from stage_inventory import admit

def inventory(root):
    result = {}
    for entry in archive_paths(root):
        if entry == ".": continue
        path = root/entry[2:]; st = path.lstat()
        row = {"mode":stat.S_IMODE(st.st_mode), "input":"synthetic-fixture"}
        if path.is_symlink(): row.update(kind="symlink",target=str(path.readlink()))
        elif path.is_dir(): row.update(kind="directory")
        else: row.update(kind="file",size=st.st_size,sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        result[entry[2:]] = row
    return {"schema":1,"objects":result}

class StageAdmission(unittest.TestCase):
    def make_root(self, root):
        for name in REQUIRED:
            p=root/name; p.parent.mkdir(parents=True,exist_ok=True)
            if name in {"usr/lib/waydroid","usr/lib/modules/"+KERNEL}: p.mkdir(exist_ok=True)
            else: p.write_bytes(b"synthetic-only");p.chmod(0o755)
    def test_all_required_inputs_admitted(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.make_root(root)
            self.assertEqual(set(plan(root,inventory(root))["requiredEntries"]),set(REQUIRED))
    def test_missing_required_even_if_inventory_complete(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.make_root(root);(root/"usr/bin/waydroid").unlink()
            with self.assertRaises(FileNotFoundError):plan(root,inventory(root))
    def test_added_unowned_file_refused(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.make_root(root);index=inventory(root);(root/"usr/foreign").write_bytes(b"x")
            with self.assertRaisesRegex(ValueError,"coverage"):plan(root,index)
    def test_changed_bytes_refused(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.make_root(root);index=inventory(root);(root/"init").write_bytes(b"changed")
            with self.assertRaises(ValueError):plan(root,index)
    def test_required_executable_mode_refused(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.make_root(root);(root/"init").chmod(0o644)
            with self.assertRaisesRegex(ValueError,"executable"):plan(root,inventory(root))
    def test_absolute_link_is_guest_rooted(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.make_root(root);p=root/"usr/bin/python3";p.unlink();p.symlink_to("/usr/bin/bash")
            self.assertEqual(plan(root,inventory(root))["requiredEntries"]["usr/bin/python3"],"usr/bin/bash")
    def test_escape_link_refused(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.make_root(root);p=root/"usr/bin/python3";p.unlink();p.symlink_to("../../../../etc/passwd")
            with self.assertRaisesRegex(ValueError,"escape"):plan(root,inventory(root))
    def test_cycle_refused(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.make_root(root);p=root/"usr/bin/python3";p.unlink();p.symlink_to("python3")
            with self.assertRaisesRegex(ValueError,"cycle"):plan(root,inventory(root))
    def test_unbound_input_refused(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);self.make_root(root);index=inventory(root);index["objects"]["init"]["input"]=""
            with self.assertRaisesRegex(ValueError,"binding"):plan(root,index)
if __name__ == "__main__":unittest.main()
