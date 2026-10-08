#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual files/pidfd/kernel lock controls; never invokes Wine or a namespace."""
import ctypes,errno,fcntl,json,os,signal,stat,subprocess,sys,tempfile,time,unittest
from pathlib import Path
from unittest.mock import patch
from owned_child import OwnedChild
from server_files import ServerFiles,lock_owner
from owned_server import OwnedServer

CHILD=r"""
import fcntl,os,signal,socket,sys,time
from pathlib import Path
folder=Path(sys.argv[1]);file=folder/'lock'
fd=os.open(file,os.O_RDWR);fcntl.lockf(fd,fcntl.LOCK_EX,1,0)
listener=socket.socket(socket.AF_UNIX);listener.bind(str(folder/'socket'))
os.chmod(folder/'socket',0o600);listener.listen(16);listener.settimeout(.05)
signal.signal(signal.SIGINT,lambda a,b:sys.exit(0))
print('READY',flush=True)
while True:
 try:c,_=listener.accept();c.close()
 except socket.timeout:pass
"""
class FilesControls(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='windows-own-files-')
        self.root=Path(self.temp.name);self.prefix=self.root/'prefix';self.prefix.mkdir(mode=0o700)
        self.files=ServerFiles(self.prefix,self.root);self.files.serverPath.parent.mkdir(mode=0o700)
        self.files.serverPath.mkdir(mode=0o700);self.lock=self.files.serverPath/'lock'
        self.lock.touch(mode=0o600);self.child=None;self.log=(self.root/'child.log').open('wb')
    def tearDown(self):
        if self.child:self.child.contain();self.child.close()
        self.log.close();self.files.close();self.temp.cleanup()
    def spawn(self):
        self.child=OwnedChild([sys.executable,'-c',CHILD,str(self.files.serverPath)],{},self.log)
        end=time.monotonic()+3
        while time.monotonic()<end:
            if 'READY' in (self.root/'child.log').read_text():break
            time.sleep(.01)
        else:self.fail('synthetic child readiness')
        self.files.acquire();return self.child
    def test_kernel_lock_peer_and_orderly_pidfd_retirement(self):
        child=self.spawn();value=self.files.listening(child.process.pid)
        self.assertEqual(value['peerPid'],child.process.pid)
        self.assertEqual(lock_owner(self.files.lockfd),child.process.pid)
        retired=child.retire(2);self.assertEqual(retired['signal'],'SIGINT')
        self.assertTrue(retired['reaped']);self.assertTrue(self.files.released()['heldLockReleased'])
    def test_actual_replacement_owner_after_retirement_refused(self):
        first=self.spawn();first.retire(2);first.close();self.child=None
        (self.files.serverPath/'socket').unlink()
        self.child=OwnedChild([sys.executable,'-c',CHILD,str(self.files.serverPath)],{},self.log)
        end=time.monotonic()+2
        while lock_owner(self.files.lockfd)!=self.child.process.pid and time.monotonic()<end:time.sleep(.01)
        with self.assertRaises(RuntimeError):self.files.released()
        self.assertFalse(self.child.dead())
    def test_socket_hardlink_refused(self):
        child=self.spawn();os.link(self.files.serverPath/'socket',self.root/'socket-link')
        with self.assertRaises(RuntimeError):self.files.listening(child.process.pid)
    def test_server_directory_symlink_refused(self):
        self.files.acquire();self.files.serverPath.rename(self.root/'oldserver')
        self.files.serverPath.symlink_to(self.root/'oldserver')
        with self.assertRaises(OSError):self.files.current_objects()
    def test_child_exits_before_atomic_observation(self):
        real=os.pidfd_open
        def delayed(pid):time.sleep(.1);return real(pid)
        with patch('owned_child.os.pidfd_open',side_effect=delayed):
            with self.assertRaises(RuntimeError):
                OwnedChild([sys.executable,'-c','pass'],{},self.log)
    def test_unlocked_refused(self):
        self.files.acquire()
        with self.assertRaises(RuntimeError):self.files.listening(os.getpid())
    def test_missing_lock_refused(self):
        self.lock.unlink()
        with self.assertRaises(FileNotFoundError):self.files.acquire()
    def test_lock_symlink_refused(self):
        self.lock.unlink();self.lock.symlink_to(self.root/'outside')
        with self.assertRaises(OSError):self.files.acquire()
    def test_lock_fifo_refused(self):
        self.lock.unlink();os.mkfifo(self.lock,0o600)
        with self.assertRaises(RuntimeError):self.files.acquire()
    def test_lock_hardlink_refused(self):
        os.link(self.lock,self.root/'other')
        with self.assertRaises(RuntimeError):self.files.acquire()
    def test_replaced_prefix_refused(self):
        self.prefix.rename(self.root/'oldprefix');self.prefix.mkdir(mode=0o700)
        with self.assertRaises(RuntimeError):self.files.acquire()
    def test_symlink_parent_refused(self):
        self.prefix.rename(self.root/'oldprefix');self.prefix.symlink_to(self.root/'oldprefix')
        with self.assertRaises(OSError):self.files.current_prefix()
    def test_replaced_server_directory_refused(self):
        self.files.acquire();self.files.serverPath.rename(self.root/'oldserver')
        self.files.serverPath.mkdir(mode=0o700)
        with self.assertRaises(RuntimeError):self.files.current_objects()
    def test_replaced_lock_refused(self):
        self.files.acquire();self.lock.unlink();self.lock.touch(mode=0o600)
        with self.assertRaises(RuntimeError):self.files.released()
    def test_group_writable_lock_refused(self):
        self.lock.chmod(0o620)
        with self.assertRaises(RuntimeError):self.files.acquire()
    def test_wrong_uid_refused(self):
        self.files.acquire()
        with patch('server_files.os.getuid',return_value=os.getuid()+1):
            with self.assertRaises(RuntimeError):self.files.current_objects()
    def test_socket_symlink_refused(self):
        child=self.spawn();sock=self.files.serverPath/'socket'
        sock.rename(self.root/'oldsocket');sock.symlink_to(self.root/'oldsocket')
        with self.assertRaises(RuntimeError):self.files.listening(child.process.pid)
    def test_wrong_lock_pid_refused(self):
        child=self.spawn()
        with self.assertRaises(RuntimeError):self.files.listening(child.process.pid+1)
    def test_wrong_peer_refused(self):
        child=self.spawn()
        with patch('server_files.struct.unpack',return_value=(child.process.pid+1,os.getuid(),os.getgid())):
            with self.assertRaises(RuntimeError):self.files.listening(child.process.pid)
    def test_unsupported_flock_abi_refused(self):
        self.files.acquire()
        with patch('server_files.platform.machine',return_value='unsupported'):
            with self.assertRaises(RuntimeError):lock_owner(self.files.lockfd)
    def test_lock_query_failure_refused(self):
        self.files.acquire();fd=os.dup(self.files.lockfd);os.close(fd)
        with self.assertRaises(OSError):lock_owner(fd)
    def test_socket_not_listening_refused(self):
        child=self.spawn();(self.files.serverPath/'socket').unlink()
        with self.assertRaises(FileNotFoundError):self.files.listening(child.process.pid)
    def test_retired_server_not_readmitted(self):
        child=self.spawn();child.retire(2)
        with self.assertRaises(RuntimeError):child.check_identity()
        self.assertTrue(child.failed)
    def test_uid_executable_mismatch_latches(self):
        child=self.spawn();child.executable='/not/the/child'
        with self.assertRaises(RuntimeError):child.check_identity()
        self.assertTrue(child.failed)
    def test_pid_starttime_mismatch_latches(self):
        child=self.spawn();child.initial['starttime']+=1
        with self.assertRaises(RuntimeError):child.check_identity()
        self.assertTrue(child.failed)
    def test_signal_refusal_not_success(self):
        child=self.spawn()
        with patch('owned_child.signal.pidfd_send_signal',side_effect=PermissionError):
            with self.assertRaises(PermissionError):child.retire(1)

class FakeChild:
    def __init__(self,*args):
        self.process=type('P',(),{'pid':101})();self.initial={'starttime':1};self.failed=False
    def check_identity(self):
        if self.failed:raise RuntimeError('initial lost')
    def retire(self,*args):return {'pid':101,'starttime':1,'exit':0,'signal':'SIGINT','pidfdDead':True,'reaped':True}
    def contain(self):return {'contained':True}
    def close(self):pass
class FakeFiles:
    def __init__(self,prefix):self.prefix=Path(prefix);self.changed=False
    def current_prefix(self):pass
    def acquire(self):pass
    def listening(self,pid):
        if self.changed:raise RuntimeError('replacement')
        return {'pid':pid}
    def released(self):return {'heldLockReleased':True,'currentLockSame':True,'replacementOwnerAbsent':True}
    def close(self):pass
class LifecycleControls(unittest.TestCase):
    def server(self):
        return OwnedServer('/synthetic/prefix','/synthetic/server',{},None,time.monotonic()+2,
                           child_type=FakeChild,files_type=FakeFiles)
    def test_actual_collaborator_positive(self):
        s=self.server();s.start();s.guard();self.assertTrue(s.retire()['qualified']);s.close()
    def test_server_death_latches_after_readiness(self):
        s=self.server();s.start();s.child.failed=True
        with self.assertRaises(RuntimeError):s.guard()
        s.child.failed=False
        with self.assertRaises(RuntimeError):s.guard()
        s.close()
    def test_replacement_never_restores_admission(self):
        s=self.server();s.start();s.files.changed=True
        with self.assertRaises(RuntimeError):s.guard()
        s.files.changed=False
        with self.assertRaises(RuntimeError):s.retire()
        s.close()
    def test_retirement_lock_failure_refuses(self):
        s=self.server();s.start()
        with patch.object(s.files,'released',side_effect=RuntimeError):
            with self.assertRaises(RuntimeError):s.retire()
        self.assertTrue(s.failed);s.close()
    def test_startup_exit_refuses(self):
        s=self.server();s.child.failed=True
        with self.assertRaises(RuntimeError):s.start()
        s.close()
    def test_forced_exit_not_normal(self):
        with tempfile.TemporaryDirectory() as root:
            with open(Path(root)/'child.log','wb') as log:
                c=OwnedChild([sys.executable,'-c','import time;time.sleep(10)'],{},log)
                with self.assertRaises(RuntimeError):c.retire(1)
                self.assertTrue(c.failed);c.close()
    def test_retirement_timeout_not_success(self):
        with tempfile.TemporaryDirectory() as root:
            with open(Path(root)/'child.log','wb') as log:
                c=OwnedChild([sys.executable,'-c',
                    'import signal,time;signal.signal(signal.SIGINT,signal.SIG_IGN);print("READY",flush=True);time.sleep(10)'],{},log)
                end=time.monotonic()+2
                while 'READY' not in (Path(root)/'child.log').read_text() and time.monotonic()<end:time.sleep(.01)
                with self.assertRaises(RuntimeError):c.retire(.05)
                self.assertTrue(c.failed);c.contain();c.close()
if __name__=='__main__':unittest.main(verbosity=2)
