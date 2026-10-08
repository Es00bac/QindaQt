# SPDX-License-Identifier: GPL-3.0-or-later
"""Two closed synthetic documents; held inode identity, never arbitrary paths."""
import hashlib,os,stat
from pathlib import Path

def document_path(root,name):
    if name not in {"app-a","app-b"}:raise RuntimeError("fixed document domain")
    return Path(root)/name/"prefix"/"qinda-fixture.txt"

def windows_document(name):
    document_path("/fixture",name)
    return "Z:\\fixture\\"+name+"\\prefix\\qinda-fixture.txt"

class PrivateDocument:
    def __init__(self,root,name):
        self.path=document_path(root,name);self.fd=None;self.parents=[]
        current=Path(root)
        for component in [None,name,"prefix"]:
            if component:current=current/component
            if component:
                try:current.mkdir(mode=0o700)
                except FileExistsError:pass
            s=current.lstat()
            if not stat.S_ISDIR(s.st_mode) or s.st_uid!=os.getuid():
                raise RuntimeError("private document parent refused")
            self.parents.append((current,s.st_dev,s.st_ino))
        self.parent=self.path.parent.stat()
        self.dirfd=os.open(self.path.parent,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
        try:self.fd=os.open(self.path.name,os.O_CREAT|os.O_EXCL|os.O_RDWR|os.O_NOFOLLOW,0o600,dir_fd=self.dirfd)
        except Exception:os.close(self.dirfd);raise
        self.initial=os.fstat(self.fd)
    def read_saved(self,marker):
        if not marker or len(marker)>64 or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789 -" for c in marker):
            raise RuntimeError("closed document text")
        for path,dev,ino in self.parents:
            s=path.lstat()
            if not stat.S_ISDIR(s.st_mode) or (s.st_dev,s.st_ino)!=(dev,ino):
                raise RuntimeError("private document ancestor changed")
        parent=self.path.parent.lstat()
        if not stat.S_ISDIR(parent.st_mode) or (parent.st_dev,parent.st_ino)!=(self.parent.st_dev,self.parent.st_ino):
            raise RuntimeError("private document parent changed")
        fd=os.open(self.path.name,os.O_RDONLY|os.O_NOFOLLOW,dir_fd=self.dirfd)
        try:
            s=os.fstat(fd)
            if not stat.S_ISREG(s.st_mode) or s.st_uid!=os.getuid() or (s.st_dev,s.st_ino)!=(self.initial.st_dev,self.initial.st_ino):
                raise RuntimeError("private document incarnation changed")
            if s.st_size>128:raise RuntimeError("private document size")
            raw=os.read(fd,129)
            # RichEdit can stream a final CRLF; no other content is admissible.
            if raw not in (marker.encode("ascii"),(marker+"\r\n").encode("ascii")):return None
            if self.path.lstat().st_ino!=s.st_ino:raise RuntimeError("private document replaced during read")
            return {"savedText":marker,"savedBytes":len(raw),"savedSHA256":hashlib.sha256(raw).hexdigest(),
                    "sameDocumentIncarnation":True,"privateDocument":str(self.path),"ctrlSObserved":True}
        finally:os.close(fd)
    def close(self):
        if self.fd is not None:os.close(self.fd);os.close(self.dirfd);self.fd=None


def admit_saved(value,marker,name):
    raw=[marker.encode("ascii"),(marker+"\r\n").encode("ascii")]
    if (value.get("privateDocument")!=str(document_path("/fixture",name))
            or value.get("savedText")!=marker or value.get("sameDocumentIncarnation") is not True
            or value.get("ctrlSObserved") is not True
            or not any(value.get("savedBytes")==len(v) and value.get("savedSHA256")==hashlib.sha256(v).hexdigest() for v in raw)):
        raise RuntimeError("private save/readback witness absent")
    return True
