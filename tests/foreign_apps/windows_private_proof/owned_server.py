#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One initial per-prefix server lifetime; never re-admit an auto-replacement."""
import errno,time
from owned_child import OwnedChild
from server_files import ServerFiles

class OwnedServer:
    def __init__(self,prefix,program,env,log,deadline,child_type=OwnedChild,files_type=ServerFiles):
        self.files=files_type(prefix);self.child=None;self.failed=False;self.ready=False
        self.deadline=deadline
        try:self.child=child_type([program,"-f","-p"],env,log)
        except Exception:self.failed=True;self.files.close();raise
    def require_time(self):
        remaining=self.deadline-time.monotonic()
        if self.failed or remaining<=0:
            self.failed=True;raise RuntimeError("owned server deadline expired")
        return remaining
    def start(self,seconds=5,monitor=lambda:None):
        if self.failed or self.ready:raise RuntimeError("initial startup cannot be re-admitted")
        self.require_time();end=min(self.deadline,time.monotonic()+seconds)
        while time.monotonic()<end:
            try:
                self.require_time();monitor();self.child.check_identity();self.files.current_prefix();self.require_time()
            except Exception:self.failed=True;raise
            try:
                self.files.acquire();value=self.files.listening(self.child.process.pid)
                self.child.check_identity();self.require_time();self.ready=True
                self.initial=value;return value
            except OSError as error:
                if error.errno not in {errno.ENOENT,errno.ECONNREFUSED}:self.failed=True;raise
                time.sleep(.02)
            except Exception:self.failed=True;raise
        self.failed=True;raise RuntimeError("initial server socket readiness timeout")
    def guard(self):
        if self.failed or not self.ready:raise RuntimeError("initial server admission lost")
        try:
            self.require_time();self.child.check_identity()
            value=self.files.listening(self.child.process.pid)
            self.child.check_identity()
            self.require_time()
            if value!=self.initial:raise RuntimeError("initial server evidence changed")
        except Exception:self.failed=True;raise
    def retire(self):
        self.guard()
        try:
            result=self.child.retire(min(5,self.require_time()))
            self.require_time()
            result.update(self.files.released());self.require_time();result["prefix"]=str(self.files.prefix)
            result["qualified"]=True;return result
        except Exception:self.failed=True;raise
    def contain(self):
        self.failed=True
        return self.child.contain() if self.child is not None else {"uncertain":True}
    def close(self):
        if self.child:self.child.close()
        self.files.close()
