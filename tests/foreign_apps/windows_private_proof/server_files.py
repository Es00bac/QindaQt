#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pinned private prefix/server objects and checked Linux x86_64 lock ABI."""
import ctypes,errno,fcntl,os,platform,socket,stat,struct
from pathlib import Path

class Flock(ctypes.Structure):
    _fields_=[("type",ctypes.c_short),("whence",ctypes.c_short),
              ("start",ctypes.c_longlong),("length",ctypes.c_longlong),
              ("pid",ctypes.c_int)]

def lock_owner(fd):
    # AGENT-GUARD: This fixture supports exactly this ABI, not guessed packing.
    if (platform.system(),platform.machine(),ctypes.sizeof(Flock),
        Flock.start.offset,Flock.length.offset,Flock.pid.offset,
        fcntl.F_GETLK,fcntl.F_WRLCK,fcntl.F_UNLCK)!=( "Linux","x86_64",32,8,16,24,5,1,2):
        raise RuntimeError("unsupported native lock ABI")
    value=Flock(fcntl.F_WRLCK,os.SEEK_SET,0,1,0)
    libc=ctypes.CDLL(None,use_errno=True)
    libc.fcntl.argtypes=[ctypes.c_int,ctypes.c_int,ctypes.POINTER(Flock)]
    libc.fcntl.restype=ctypes.c_int
    if libc.fcntl(fd,fcntl.F_GETLK,ctypes.byref(value))<0:
        raise OSError(ctypes.get_errno(),"lock query failed")
    if value.type==fcntl.F_UNLCK:return None
    if value.type!=fcntl.F_WRLCK or value.pid<=0 or value.whence!=os.SEEK_SET or value.start!=0 or value.length!=1:raise RuntimeError("unexpected lock result")
    return value.pid

def fingerprint(info):
    return (info.st_dev,info.st_ino,info.st_uid,info.st_mode & 0o177777)

def check(info,kind):
    if info.st_uid!=os.getuid() or stat.S_IFMT(info.st_mode)!=kind or info.st_mode&0o022:
        raise RuntimeError("private object owner/type/mode mismatch")
    if kind in {stat.S_IFREG,stat.S_IFSOCK} and info.st_nlink!=1:raise RuntimeError("private lock hardlink refused")
    return fingerprint(info)

def directory(path):
    # Open each component with no-follow; no resolution through symlink parents.
    path=Path(path)
    if not path.is_absolute() or ".." in path.parts:raise RuntimeError("private directory path")
    fd=os.open("/",os.O_RDONLY|os.O_DIRECTORY|os.O_CLOEXEC)
    try:
        for name in path.parts[1:]:
            nxt=os.open(name,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW|os.O_CLOEXEC,dir_fd=fd)
            os.close(fd);fd=nxt
        return fd
    except Exception:
        os.close(fd);raise

class ServerFiles:
    def __init__(self,prefix,tmp=Path("/tmp")):
        self.prefix=Path(prefix);self.tmp=Path(tmp);self.prefixfd=directory(prefix)
        try:self.prefixIdentity=check(os.fstat(self.prefixfd),stat.S_IFDIR)
        except Exception:os.close(self.prefixfd);self.prefixfd=None;raise
        self.serverPath=self.tmp/(".wine-"+str(os.getuid()))/(
            "server-"+format(self.prefixIdentity[0],"x")+"-"+format(self.prefixIdentity[1],"x"))
        self.serverfd=None;self.lockfd=None;self.serverIdentity=None;self.lockIdentity=None;self.socketIdentity=None
    def current_prefix(self):
        fd=directory(self.prefix)
        try:
            if check(os.fstat(fd),stat.S_IFDIR)!=self.prefixIdentity:raise RuntimeError("prefix incarnation changed")
        finally:os.close(fd)
    def acquire(self):
        self.current_prefix()
        basefd=directory(self.serverPath.parent)
        try:check(os.fstat(basefd),stat.S_IFDIR)
        finally:os.close(basefd)
        if self.serverfd is None:
            self.serverfd=directory(self.serverPath)
            self.serverIdentity=check(os.fstat(self.serverfd),stat.S_IFDIR)
        if self.lockfd is None:
            self.lockfd=os.open("lock",os.O_RDONLY|os.O_NONBLOCK|os.O_NOFOLLOW|os.O_CLOEXEC,dir_fd=self.serverfd)
            self.lockIdentity=check(os.fstat(self.lockfd),stat.S_IFREG)
        self.current_objects()
    def current_objects(self):
        self.current_prefix();fd=directory(self.serverPath)
        try:
            if check(os.fstat(fd),stat.S_IFDIR)!=self.serverIdentity:raise RuntimeError("server directory replaced")
            info=os.stat("lock",dir_fd=fd,follow_symlinks=False)
            if check(info,stat.S_IFREG)!=self.lockIdentity:raise RuntimeError("current lock replaced")
            if check(os.fstat(self.lockfd),stat.S_IFREG)!=self.lockIdentity:raise RuntimeError("held lock changed")
        finally:os.close(fd)
    def listening(self,pid):
        self.current_objects()
        if lock_owner(self.lockfd)!=pid:raise RuntimeError("initial lock owner lost")
        info=os.stat("socket",dir_fd=self.serverfd,follow_symlinks=False)
        identity=check(info,stat.S_IFSOCK)
        if self.socketIdentity is not None and identity!=self.socketIdentity:
            raise RuntimeError("server socket replaced")
        client=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        try:
            client.settimeout(.2);client.connect(str(self.serverPath/"socket"))
            peer=struct.unpack("3i",client.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12))
            if peer[0]!=pid or peer[1]!=os.getuid():raise RuntimeError("server peer mismatch")
        finally:client.close()
        self.current_objects()
        if lock_owner(self.lockfd)!=pid:raise RuntimeError("lock changed during peer observation")
        if check(os.stat("socket",dir_fd=self.serverfd,follow_symlinks=False),stat.S_IFSOCK)!=identity:
            raise RuntimeError("socket changed during peer observation")
        self.socketIdentity=identity
        return {"prefix":str(self.prefix),"prefixIdentity":self.prefixIdentity,
                "serverDirectory":self.serverIdentity,"lock":self.lockIdentity,
                "socket":identity,"peerPid":peer[0],"peerUid":peer[1]}
    def released(self):
        self.current_objects()
        if lock_owner(self.lockfd) is not None:raise RuntimeError("held/current lock still owned or replaced")
        try:
            current=check(os.stat("socket",dir_fd=self.serverfd,follow_symlinks=False),stat.S_IFSOCK)
            if self.socketIdentity is not None and current!=self.socketIdentity:
                raise RuntimeError("replacement socket after retirement")
        except FileNotFoundError:pass
        return {"heldLockReleased":True,"currentLockSame":True,"replacementOwnerAbsent":True}
    def close(self):
        for name in ["lockfd","serverfd","prefixfd"]:
            fd=getattr(self,name)
            if fd is not None:os.close(fd);setattr(self,name,None)
