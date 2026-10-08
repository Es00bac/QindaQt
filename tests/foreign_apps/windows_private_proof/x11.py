# SPDX-License-Identifier: GPL-3.0-or-later
"""Read actual private X11 clients and XRes-local PID; no title/PID-property authority."""
import ctypes as C,os
U=C.c_ulong
class Client(C.Structure):
    _fields_=[("base",U),("mask",U)]
class Spec(C.Structure):
    _fields_=[("client",U),("mask",C.c_uint)]
class Value(C.Structure):
    _fields_=[("spec",Spec),("length",C.c_long),("value",C.c_void_p)]

class MessageData(C.Union):
    _fields_=[("longs",C.c_long*5),("bytes",C.c_char*20)]
class Message(C.Structure):
    _fields_=[("type",C.c_int),("serial",U),("send_event",C.c_int),("display",C.c_void_p),("window",U),("message_type",U),("format",C.c_int),("data",MessageData)]
class Event(C.Union):
    _fields_=[("message",Message),("pad",C.c_long*24)]

class X11:
    def __init__(self):
        if not os.environ.get("DISPLAY"):raise RuntimeError("private DISPLAY absent")
        self.x=C.CDLL("libX11.so.6");self.res=C.CDLL("libXRes.so.1")
        self.x.XOpenDisplay.argtypes=[C.c_char_p];self.x.XOpenDisplay.restype=C.c_void_p
        self.d=self.x.XOpenDisplay(os.environ["DISPLAY"].encode())
        if not self.d:raise RuntimeError("private X11 connect failed")
        self.x.XDefaultRootWindow.argtypes=[C.c_void_p];self.x.XDefaultRootWindow.restype=U
        self.root=self.x.XDefaultRootWindow(self.d)
        self.x.XInternAtom.argtypes=[C.c_void_p,C.c_char_p,C.c_int];self.x.XInternAtom.restype=U
        self.x.XGetWindowProperty.argtypes=[C.c_void_p,U,U,C.c_long,C.c_long,C.c_int,U,C.POINTER(U),C.POINTER(C.c_int),C.POINTER(U),C.POINTER(U),C.POINTER(C.POINTER(C.c_ubyte))]
        self.x.XGetWindowProperty.restype=C.c_int
        self.x.XFree.argtypes=[C.c_void_p]
        self.x.XGetGeometry.argtypes=[C.c_void_p,U,C.POINTER(U),C.POINTER(C.c_int),C.POINTER(C.c_int),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint),C.POINTER(C.c_uint)]
        self.x.XGetGeometry.restype=C.c_int
        self.x.XResizeWindow.argtypes=[C.c_void_p,U,C.c_uint,C.c_uint]
        self.x.XSendEvent.argtypes=[C.c_void_p,U,C.c_int,C.c_long,C.POINTER(Event)]
        self.x.XFlush.argtypes=[C.c_void_p]
        self.x.XCloseDisplay.argtypes=[C.c_void_p]
        self.res.XResQueryClients.argtypes=[C.c_void_p,C.POINTER(C.c_int),C.POINTER(C.POINTER(Client))];self.res.XResQueryClients.restype=C.c_int
        self.res.XResQueryVersion.argtypes=[C.c_void_p,C.POINTER(C.c_int),C.POINTER(C.c_int)];self.res.XResQueryVersion.restype=C.c_int
        self.res.XResQueryClientIds.argtypes=[C.c_void_p,C.c_long,C.POINTER(Spec),C.POINTER(C.c_long),C.POINTER(C.POINTER(Value))];self.res.XResQueryClientIds.restype=C.c_int
        self.res.XResGetClientPid.argtypes=[C.POINTER(Value)];self.res.XResGetClientPid.restype=C.c_int
        self.res.XResClientIdsDestroy.argtypes=[C.c_long,C.POINTER(Value)]
        major=C.c_int();minor=C.c_int()
        if not self.res.XResQueryVersion(self.d,C.byref(major),C.byref(minor)) or (major.value,minor.value)<(1,2):
            self.x.XCloseDisplay(self.d)
            raise RuntimeError("XRes1.2 local PID unavailable; no property fallback")
        self.version=[major.value,minor.value]

    def property32(self,window,name):
        atom=self.x.XInternAtom(self.d,name.encode(),0)
        kind=U();fmt=C.c_int();count=U();after=U();data=C.POINTER(C.c_ubyte)()
        code=self.x.XGetWindowProperty(self.d,window,atom,0,1024,0,0,C.byref(kind),C.byref(fmt),C.byref(count),C.byref(after),C.byref(data))
        try:
            if code or after.value or fmt.value!=32 or count.value>1024:return []
            return list(C.cast(data,C.POINTER(U))[:count.value]) if data else []
        finally:
            if data:self.x.XFree(data)

    def windows(self):
        return self.property32(self.root,"_NET_CLIENT_LIST")

    def normal_window(self,window):
        normal=self.x.XInternAtom(self.d,b"_NET_WM_WINDOW_TYPE_NORMAL",0)
        return self.property32(window,"_NET_WM_WINDOW_TYPE")==[normal]

    def local_pid(self,window):
        count=C.c_int();clients=C.POINTER(Client)()
        if not self.res.XResQueryClients(self.d,C.byref(count),C.byref(clients)):return None
        try:
            if not 0<count.value<=4096:raise RuntimeError("XRes client bound")
            matches=[v.base for v in clients[:count.value] if (window & ~v.mask)==v.base]
        finally:
            if clients:self.x.XFree(clients)
        if len(matches)!=1:return None
        spec=Spec(matches[0],2);length=C.c_long();values=C.POINTER(Value)()
        # Installed libXRes returns Success=0 for this newer API (unlike QueryClients).
        if self.res.XResQueryClientIds(self.d,1,C.byref(spec),C.byref(length),C.byref(values))!=0:return None
        try:
            if not 0<length.value<=8:raise RuntimeError("XRes ID bound")
            pids=[self.res.XResGetClientPid(C.byref(values[i])) for i in range(length.value) if values[i].spec.mask & 2]
            return pids[0] if len(pids)==1 and pids[0]>1 else None
        finally:
            if values:self.res.XResClientIdsDestroy(length,values)

    def geometry(self,window):
        root=U();x=C.c_int();y=C.c_int();w=C.c_uint();h=C.c_uint();border=C.c_uint();depth=C.c_uint()
        if not self.x.XGetGeometry(self.d,window,C.byref(root),C.byref(x),C.byref(y),C.byref(w),C.byref(h),C.byref(border),C.byref(depth)):
            return None
        return [x.value,y.value,w.value,h.value]
    def resize(self,window,width,height):
        self.x.XResizeWindow(self.d,window,width,height);self.x.XFlush(self.d)
    def request_close(self,window):
        event=Event()
        event.message=Message(33,0,1,self.d,window,self.x.XInternAtom(self.d,b"WM_PROTOCOLS",0),32,MessageData())
        event.message.data.longs[0]=self.x.XInternAtom(self.d,b"WM_DELETE_WINDOW",0)
        if not self.x.XSendEvent(self.d,window,0,0,C.byref(event)):raise RuntimeError("private close event refused")
        self.x.XFlush(self.d)
    def close(self):
        self.x.XCloseDisplay(self.d)
