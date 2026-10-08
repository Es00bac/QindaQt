"""Pure socket/lifetime admission controls; no child, socket or audio service."""
import stat
import struct
import unittest
from types import SimpleNamespace
from unittest.mock import MagicMock, patch
import guest

class Admission(unittest.TestCase):
    def check_case(self, *, peer=(42,1000,1000), uid=1000, mode=stat.S_IFSOCK,
                   birth="11", replaced=False, wrong_exe=False, dead=False):
        child=MagicMock(pid=42);child.poll.return_value=0 if dead else None
        path=MagicMock();path.lstat.side_effect=[
            SimpleNamespace(st_mode=mode,st_uid=uid,st_dev=1,st_ino=2),
            SimpleNamespace(st_mode=mode,st_uid=uid,st_dev=1,st_ino=3 if replaced else 2)]
        connection=MagicMock();connection.__enter__.return_value=connection
        connection.getsockopt.return_value=struct.pack("3i",*peer)
        with patch.object(guest,"wait_path"), patch.object(guest.socket,"socket",return_value=connection), patch.object(guest,"process_start",return_value=birth), patch.object(guest.os,"stat",side_effect=[SimpleNamespace(st_dev=1,st_ino=4),SimpleNamespace(st_dev=1,st_ino=5 if wrong_exe else 4)]):
            guest.owned_audio_socket(child,path,"11")
    def test_exact_owned_endpoint(self):self.check_case()
    def test_refusals(self):
        for case in [dict(peer=(43,1000,1000)),dict(peer=(42,0,1000)),
                     dict(peer=(42,1000,0)),dict(uid=0),dict(mode=stat.S_IFREG),
                     dict(birth="12"),dict(replaced=True),dict(wrong_exe=True),
                     dict(dead=True)]:
            with self.subTest(case=case),self.assertRaises(RuntimeError):self.check_case(**case)

if __name__=="__main__":unittest.main()
