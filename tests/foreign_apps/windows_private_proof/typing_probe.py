# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual private-display XTest typing, clipboard readback and pixel witnesses.

Borrowed X11 display and current-incarnation guard outlive this synchronous
probe. Only fixed synthetic ASCII enters the two trusted fixture apps.
Clipboard readback and changed pixels are separate witnesses, not OCR,
physical keyboard evidence or authority for a platform badge.
"""
import ctypes as C,hashlib,time
from pathlib import Path
U=C.c_ulong

class Selection(C.Structure):
    _fields_=[("type",C.c_int),("serial",U),("send_event",C.c_int),
              ("display",C.c_void_p),("requestor",U),("selection",U),
              ("target",U),("property",U),("time",U)]
class Event(C.Union):
    _fields_=[("selection",Selection),("pad",C.c_long*24)]
class Image(C.Structure):
    _fields_=[("width",C.c_int),("height",C.c_int),("xoffset",C.c_int),
              ("format",C.c_int),("data",C.c_void_p),("byte_order",C.c_int),
              ("bitmap_unit",C.c_int),("bitmap_bit_order",C.c_int),
              ("bitmap_pad",C.c_int),("depth",C.c_int),("bytes_per_line",C.c_int),
              ("bits_per_pixel",C.c_int),("red_mask",U),("green_mask",U),
              ("blue_mask",U)]

def admit_typing(value,marker):
    if (value.get("clipboardText")!=marker or value.get("focusWithinClient") is not True
            or value.get("beforeSHA256")==value.get("afterSHA256")
            or value.get("pixelBytes",0)<=0):
        raise RuntimeError("actual typing/readback/pixel witnesses absent")
    return value

def probe(io,window,marker,guard,deadline,output):
    if not marker or len(marker)>64 or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789 -" for c in marker):
        raise RuntimeError("closed synthetic typing alphabet")
    def checked():
        if time.monotonic()>=deadline:raise RuntimeError("typing deadline")
        guard()
    checked();io.focus(window);checked()
    io.chord(window,"a");io.key(window,0xff08) # BackSpace: fresh blank editor.
    before=io.capture(window,output/"before.ppm")
    for char in marker:
        checked();io.key(window,ord(char))
    io.flush();time.sleep(.1);checked()
    after=io.capture(window,output/"after.ppm")
    io.chord(window,"a");io.chord(window,"c");checked()
    text=io.clipboard(min(deadline,time.monotonic()+3),checked)
    checked()
    return admit_typing({"clipboardText":text,"focusWithinClient":io.focus_within(window),
        "beforeSHA256":before["sha256"],"afterSHA256":after["sha256"],
        "pixelBytes":after["pixelBytes"],"captures":[before,after],
        "physicalInput":False,"renderedTextOCR":False},marker)

class TypingIO:
    def __init__(self,x):
        self.x=x.x;self.d=x.d;self.root=x.root
        self.xt=C.CDLL("libXtst.so.6")
        signatures={
            "XRaiseWindow":([C.c_void_p,U],C.c_int),
            "XSetInputFocus":([C.c_void_p,U,C.c_int,U],C.c_int),
            "XGetInputFocus":([C.c_void_p,C.POINTER(U),C.POINTER(C.c_int)],C.c_int),
            "XQueryTree":([C.c_void_p,U,C.POINTER(U),C.POINTER(U),C.POINTER(C.POINTER(U)),C.POINTER(C.c_uint)],C.c_int),
            "XKeysymToKeycode":([C.c_void_p,U],C.c_ubyte),
            "XCreateSimpleWindow":([C.c_void_p,U,C.c_int,C.c_int,C.c_uint,C.c_uint,C.c_uint,U,U],U),
            "XDestroyWindow":([C.c_void_p,U],C.c_int),
            "XConvertSelection":([C.c_void_p,U,U,U,U,U],C.c_int),
            "XPending":([C.c_void_p],C.c_int),
            "XNextEvent":([C.c_void_p,C.POINTER(Event)],C.c_int),
            "XGetImage":([C.c_void_p,U,C.c_int,C.c_int,C.c_uint,C.c_uint,U,C.c_int],C.POINTER(Image)),
            "XGetPixel":([C.POINTER(Image),C.c_int,C.c_int],U),
            "XDestroyImage":([C.POINTER(Image)],C.c_int)}
        for name,(args,result) in signatures.items():
            f=getattr(self.x,name);f.argtypes=args;f.restype=result
        self.xt.XTestQueryExtension.argtypes=[C.c_void_p,*([C.POINTER(C.c_int)]*4)]
        self.xt.XTestQueryExtension.restype=C.c_int
        self.xt.XTestFakeKeyEvent.argtypes=[C.c_void_p,C.c_uint,C.c_int,U]
        self.xt.XTestFakeKeyEvent.restype=C.c_int
        values=[C.c_int() for _ in range(4)]
        if not self.xt.XTestQueryExtension(self.d,*(C.byref(v) for v in values)):
            raise RuntimeError("private XTest unavailable")
    def flush(self):self.x.XFlush(self.d)
    def focus_within(self,window):
        focus=U();revert=C.c_int();self.x.XGetInputFocus(self.d,C.byref(focus),C.byref(revert))
        current=focus.value
        for _ in range(16):
            if current==window:return True
            if current in (0,1,self.root):return False
            root=U();parent=U();children=C.POINTER(U)();count=C.c_uint()
            try:
                if not self.x.XQueryTree(self.d,current,C.byref(root),C.byref(parent),C.byref(children),C.byref(count)):return False
                current=parent.value
            finally:
                if children:self.x.XFree(children)
        return False
    def focus(self,window):
        self.x.XRaiseWindow(self.d,window);self.x.XSetInputFocus(self.d,window,2,0);self.flush()
        if not self.focus_within(window):raise RuntimeError("private app focus refused")
    def code(self,symbol):
        value=self.x.XKeysymToKeycode(self.d,symbol)
        if not value:raise RuntimeError("synthetic key unavailable")
        return value
    def key(self,window,symbol):
        if not self.focus_within(window):raise RuntimeError("private app focus changed")
        code=self.code(symbol)
        if not self.xt.XTestFakeKeyEvent(self.d,code,1,0) or not self.xt.XTestFakeKeyEvent(self.d,code,0,0):
            raise RuntimeError("XTest key refused")
        self.flush()
    def chord(self,window,char):
        if not self.focus_within(window):raise RuntimeError("private app focus changed")
        control=self.code(0xffe3)
        if not self.xt.XTestFakeKeyEvent(self.d,control,1,0):raise RuntimeError("XTest modifier refused")
        try:self.key(window,ord(char))
        finally:self.xt.XTestFakeKeyEvent(self.d,control,0,0);self.flush()
    def clipboard(self,deadline,guard):
        request=self.x.XCreateSimpleWindow(self.d,self.root,0,0,1,1,0,0,0)
        selection=self.x.XInternAtom(self.d,b"CLIPBOARD",0)
        target=self.x.XInternAtom(self.d,b"UTF8_STRING",0)
        prop=self.x.XInternAtom(self.d,b"QINDA_FIXTURE_TYPED_TEXT",0)
        try:
            self.x.XConvertSelection(self.d,selection,target,prop,request,0);self.flush()
            events=0
            while time.monotonic()<deadline:
                guard()
                while self.x.XPending(self.d):
                    event=Event();self.x.XNextEvent(self.d,C.byref(event));events+=1
                    if events>256:raise RuntimeError("clipboard event bound")
                    v=event.selection
                    if v.type!=31 or v.requestor!=request:continue
                    if v.selection!=selection or v.target!=target or v.property!=prop:
                        raise RuntimeError("private clipboard conversion refused")
                    kind=U();fmt=C.c_int();count=U();remaining=U();data=C.POINTER(C.c_ubyte)()
                    try:
                        code=self.x.XGetWindowProperty(self.d,request,prop,0,1024,1,0,
                            C.byref(kind),C.byref(fmt),C.byref(count),C.byref(remaining),C.byref(data))
                        if code or kind.value!=target or fmt.value!=8 or remaining.value or count.value>4096:
                            raise RuntimeError("private clipboard text bound/type")
                        return C.string_at(data,count.value).decode("utf-8").rstrip("\0")
                    finally:
                        if data:self.x.XFree(data)
                time.sleep(.01)
            raise RuntimeError("private clipboard deadline")
        finally:self.x.XDestroyWindow(self.d,request)
    def capture(self,window,path):
        image=self.x.XGetImage(self.d,window,8,40,240,96,U(-1),2)
        if not image:raise RuntimeError("private pixel capture absent")
        try:
            info=image.contents
            if (info.width,info.height)!=(240,96) or (info.red_mask,info.green_mask,info.blue_mask)!=(0xff0000,0xff00,0xff):
                raise RuntimeError("private pixel geometry/visual refused")
            pixels=bytearray()
            for y in range(96):
                for x in range(240):
                    value=self.x.XGetPixel(image,x,y);pixels.extend(((value>>16)&255,(value>>8)&255,value&255))
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(b"P6\n240 96\n255\n"+pixels)
            return {"path":str(path),"sha256":hashlib.sha256(path.read_bytes()).hexdigest(),"pixelBytes":len(pixels)}
        finally:self.x.XDestroyImage(image)
