# SPDX-License-Identifier: GPL-3.0-or-later
"""Owned filesystem controls only; no app, display or subprocess."""
import os,tempfile,unittest
from pathlib import Path
from private_document import PrivateDocument,document_path,windows_document,admit_saved
class Documents(unittest.TestCase):
    def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.doc=None
    def tearDown(self):
        if self.doc:self.doc.close()
        self.tmp.cleanup()
    def test_fixed_argv(self):
        self.assertEqual(windows_document("app-a"),"Z:\\fixture\\app-a\\prefix\\qinda-fixture.txt")
        self.assertRaises(RuntimeError,windows_document,"../foreign")
    def test_save_exact_inode_ascii_and_crlf(self):
        self.doc=PrivateDocument(self.root,"app-a")
        for text in ["qinda a 3108","qinda a 3108\r\n"]:
            self.doc.path.write_bytes(text.encode())
            v=self.doc.read_saved("qinda a 3108")
            self.assertTrue(v["sameDocumentIncarnation"]);self.assertEqual(v["savedBytes"],len(text))
    def test_preexisting_and_symlink_refused(self):
        path=document_path(self.root,"app-a");path.parent.mkdir(parents=True)
        for symlink in [False,True]:
            if symlink:path.symlink_to(self.root/"foreign")
            else:path.write_text("foreign")
            self.assertRaises(FileExistsError,PrivateDocument,self.root,"app-a")
            path.unlink()
    def test_replacement_refused(self):
        self.doc=PrivateDocument(self.root,"app-a");self.doc.path.unlink();self.doc.path.write_text("qinda a 3108")
        self.assertRaises(RuntimeError,self.doc.read_saved,"qinda a 3108")
    def test_symlink_replacement_refused(self):
        self.doc=PrivateDocument(self.root,"app-a");self.doc.path.unlink();self.doc.path.symlink_to(self.root/"foreign")
        self.assertRaises(OSError,self.doc.read_saved,"qinda a 3108")
    def test_unsaved_or_foreign_content_and_closed_text(self):
        self.doc=PrivateDocument(self.root,"app-a");self.assertIsNone(self.doc.read_saved("qinda a 3108"))
        self.doc.path.write_text("other");self.assertIsNone(self.doc.read_saved("qinda a 3108"))
        self.assertRaises(RuntimeError,self.doc.read_saved,"bad\n")
    def test_saved_receipt_refuses_false_or_foreign(self):
        self.doc=PrivateDocument(self.root,"app-a");self.doc.path.write_text("qinda a 3108")
        v=self.doc.read_saved("qinda a 3108");v["privateDocument"]="/fixture/app-a/prefix/qinda-fixture.txt"
        self.assertTrue(admit_saved(v,"qinda a 3108","app-a"))
        for key,bad in [("savedBytes",0),("savedSHA256","a"*64),("privateDocument","/host/file"),("ctrlSObserved",False)]:
            changed=dict(v);changed[key]=bad;self.assertRaises(RuntimeError,admit_saved,changed,"qinda a 3108","app-a")
if __name__=="__main__":unittest.main()
