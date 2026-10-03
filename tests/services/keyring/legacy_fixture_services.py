# SPDX-License-Identifier: GPL-3.0-or-later
"""Private synthetic providers. No file readers, credentials or source mutation API."""
import dbus
import dbus.service
from gi.repository import GLib
PROPERTIES="org.freedesktop.DBus.Properties"
SERVICE="org.freedesktop.Secret.Service"
COLLECTION="org.freedesktop.Secret.Collection"
ITEM="org.freedesktop.Secret.Item"
PROMPT="org.freedesktop.Secret.Prompt"
ROOT="/org/freedesktop/secrets"
KW="org.kde.KWallet"
class Properties(dbus.service.Object):
    def __init__(self,fixture,path):
        self.fixture=fixture;self.path=path;super().__init__(fixture.ss,path)
    @dbus.service.method(PROPERTIES,in_signature="ss",out_signature="v",async_callbacks=("reply","error"),message_keyword="request")
    def Get(self,interface,name,reply,error,request=None):
        f=self.fixture;f.calls.append("Get")
        value=f.property(self.path,str(interface),str(name))
        if f.mode=="malformed" and name=="Created": value=dbus.String("wrong",variant_level=1)
        if f.mode=="forged-metadata" and name=="Label" and self.path=="/legacy/login/password":
            f.forge(request,[dbus.String("forged",variant_level=1)],"v");GLib.timeout_add(300,lambda:(reply(value),False)[1]);return
        reply(value)
class Session(dbus.service.Object):
    def __init__(self,fixture,path): self.fixture=fixture;super().__init__(fixture.ss,path)
    @dbus.service.method("org.freedesktop.Secret.Session",in_signature="",out_signature="")
    def Close(self): self.fixture.calls.append("Close")
class Prompt(dbus.service.Object):
    def __init__(self,fixture): self.fixture=fixture;super().__init__(fixture.ss,"/legacy/prompt")
    @dbus.service.method(PROMPT,in_signature="s",out_signature="",message_keyword="request")
    def Prompt(self,window,request=None):
        f=self.fixture;f.calls.append("Prompt")
        paths=dbus.Array(f.prompt_paths,signature="o")
        if f.mode=="forged-completed":
            signal=dbus.lowlevel.SignalMessage("/legacy/prompt",PROMPT,"Completed")
            signal.append(False,dbus.Array(paths,signature="o",variant_level=1),signature="bv");f.attacker.send_message(signal)
        def complete():
            cancelled=f.mode in ("cancel","forged-completed")
            if not cancelled:
                f.locked=False
                signal=dbus.lowlevel.SignalMessage("/legacy/collection/login",PROPERTIES,"PropertiesChanged")
                signal.append(COLLECTION,dbus.Dictionary({"Locked":dbus.Boolean(False,variant_level=1)},signature="sv"),dbus.Array([],signature="s"),signature="sa{sv}as");f.ss.send_message(signal)
            self.Completed(cancelled,dbus.Array([] if cancelled else paths,signature="o",variant_level=1));return False
        GLib.timeout_add(50,complete)
    @dbus.service.method(PROMPT,in_signature="",out_signature="")
    def Dismiss(self): self.fixture.calls.append("Dismiss")
    @dbus.service.signal(PROMPT,signature="bv")
    def Completed(self,cancelled,result): pass
class Secret(Properties):
    def __init__(self,fixture): super().__init__(fixture,ROOT)
    @dbus.service.method(SERVICE,in_signature="sv",out_signature="vo")
    def OpenSession(self,algorithm,input):
        f=self.fixture;f.calls.append("OpenSession")
        if algorithm!="plain": raise dbus.exceptions.DBusException("Unsupported",name="org.freedesktop.DBus.Error.NotSupported")
        f.sessions+=1;path="/legacy/session/s"+str(f.sessions);f.objects.append(Session(f,path));return dbus.String("",variant_level=1),dbus.ObjectPath(path)
    @dbus.service.method(SERVICE,in_signature="ao",out_signature="aoo")
    def Unlock(self,objects):
        f=self.fixture;f.calls.append("Unlock")
        if f.mode=="unlock-failure": raise dbus.exceptions.DBusException("Refused",name="org.freedesktop.Secret.Error.IsLocked")
        if f.locked:
            f.prompt_paths=list(objects);return dbus.Array([],signature="o"),dbus.ObjectPath("/legacy/prompt")
        return objects,dbus.ObjectPath("/")
    @dbus.service.method(SERVICE,in_signature="s",out_signature="o")
    def ReadAlias(self,alias):
        self.fixture.calls.append("ReadAlias");return dbus.ObjectPath({"default":"/legacy/collection/login","alternate":"/legacy/collection/gogcli"}.get(str(alias),"/"))
    @dbus.service.method(SERVICE,in_signature="aoo",out_signature="a{o(oayays)}",async_callbacks=("reply","error"),message_keyword="request")
    def GetSecrets(self,items,session,reply,error,request=None):
        f=self.fixture;f.calls.append("GetSecrets");f.secret_calls+=1
        if f.mode=="rate-limit":
            for _ in range(65): f.ss.send_message(dbus.lowlevel.SignalMessage(ROOT,SERVICE,"Ignored"))
        values={}
        for path in items:
            value=f.items[str(path)][2]
            if f.mode=="oversize": value=b"X"*(1024*1024+1)
            if f.mode=="mutation" and f.secret_calls>3 and value: value=bytes([value[0]^1])+value[1:]
            values[dbus.ObjectPath(path)]=dbus.Struct((session,dbus.ByteArray(b""),dbus.ByteArray(value),"application/octet-stream"),signature="oayays")
        result=dbus.Dictionary(values,signature="o(oayays)")
        if f.mode=="forged-secret":
            forged=dbus.Dictionary({dbus.ObjectPath(path):dbus.Struct((session,dbus.ByteArray(b""),dbus.ByteArray(b"synthetic-foreign"),"application/octet-stream"),signature="oayays") for path in items},signature="o(oayays)")
            f.forge(request,[forged],"a{o(oayays)}");GLib.timeout_add(300,lambda:(reply(result),False)[1]);return
        if f.mode=="replacement":
            f.replacement.request_name("org.freedesktop.secrets",dbus.bus.NAME_FLAG_REPLACE_EXISTING)
        if f.mode=="missing-secret": result=dbus.Dictionary({},signature="o(oayays)")
        reply(result)
class Wallet(dbus.service.Object):
    def __init__(self,fixture): self.fixture=fixture;super().__init__(fixture.kw,"/modules/kwalletd6")
    @dbus.service.method(KW,in_signature="",out_signature="as")
    def wallets(self):
        self.fixture.calls.append("wallets")
        return ["kdewallet"]*(2 if self.fixture.mode=="kw-duplicate-wallets" else 1)
    @dbus.service.method(KW,in_signature="sxsb",out_signature="i")
    def openAsync(self,wallet,window,application,session):
        f=self.fixture;f.calls.append("openAsync");f.opened+=1;transaction=f.opened;handle=100+transaction
        if wallet!="kdewallet" or application!="org.qindaqt.KeyringImport" or not session: raise RuntimeError("Invalid private open")
        GLib.timeout_add(25,lambda:(self.walletAsyncOpened(transaction,-1 if f.mode=="kw-cancel" else handle),False)[1]);return transaction
    @dbus.service.signal(KW,signature="ii")
    def walletAsyncOpened(self,transaction,handle): pass
    @dbus.service.method(KW,in_signature="is",out_signature="as")
    def folderList(self,handle,application):
        f=self.fixture;f.calls.append("folderList");folders=sorted(f.folders)
        if f.mode=="kw-folder-capacity": return [folders[0]]*1025
        if f.mode=="kw-folder-mutation" and f.calls.count("folderList")>1: return folders[:-1]*2
        return folders*2 if f.mode in ("kw-duplicate-folders","kw-folder-mutation") else folders
    @dbus.service.method(KW,in_signature="iss",out_signature="as")
    def entryList(self,handle,folder,application):
        self.fixture.calls.append("entryList");entries=sorted(self.fixture.folders[str(folder)])
        return entries*2 if self.fixture.mode=="kw-duplicate-entries" else entries
    @dbus.service.method(KW,in_signature="isss",out_signature="i")
    def entryType(self,handle,folder,key,application): self.fixture.calls.append("entryType");return self.fixture.folders[str(folder)][str(key)][0]
    @dbus.service.method(KW,in_signature="isss",out_signature="ay")
    def readEntry(self,handle,folder,key,application): self.fixture.calls.append("readEntry");return dbus.ByteArray(self.fixture.folders[str(folder)][str(key)][1])
    @dbus.service.method(KW,in_signature="ibs",out_signature="i")
    def close(self,handle,force,application):
        if force or application!="org.qindaqt.KeyringImport": raise RuntimeError("Invalid private close")
        self.fixture.calls.append("close");return 0
class Fixture:
    def __init__(self,address):
        self.ss=dbus.bus.BusConnection(address);self.kw=dbus.bus.BusConnection(address)
        self.attacker=dbus.bus.BusConnection(address);self.replacement=dbus.bus.BusConnection(address)
        self.ss.request_name("org.freedesktop.secrets",dbus.bus.NAME_FLAG_ALLOW_REPLACEMENT)
        self.kw.request_name("org.kde.kwalletd6")
        self.mode="normal";self.locked=False;self.calls=[];self.sessions=0;self.secret_calls=0;self.opened=0
        self.prompt_paths=[]
        self.collections=["/legacy/collection/login","/legacy/collection/gogcli","/legacy/collection/session"]
        self.items={"/legacy/login/password":("Password",{"app":"gogcli"},bytes([0,255,16])),
            "/legacy/login/empty":("Empty",{},b""),"/legacy/gogcli/account":("Account",{"service":"gogcli"},bytes((n*17)&255 for n in range(64)))}
        self.folders={"Passwords":{"same":(1,b"\x00P\x00w\x00")},"Maps":{"same":(3,bytes([0,0,0,1,255,65]))},
            "Streams":{"same":(2,bytes([0,255,1,7]))},"Empty":{},"xdg-desktop-portal":{"org.example.Legacy":(2,bytes(range(64)))}}
        self.objects=[Secret(self),Prompt(self),Wallet(self)]
        for path in self.collections+list(self.items): self.objects.append(Properties(self,path))
    def property(self,path,interface,name):
        if interface==SERVICE and name=="Collections": return dbus.Array(self.collections+([self.collections[0]] if self.mode=="duplicate" else []),signature="o",variant_level=1)
        if name=="Locked": return dbus.Boolean(self.locked,variant_level=1)
        if name in ("Created","Modified"): return dbus.UInt64(1 if name=="Created" else 2,variant_level=1)
        if interface==COLLECTION:
            leaf=path.rsplit("/",1)[-1]
            if name=="Label": return dbus.String(leaf,variant_level=1)
            if name=="Items": return dbus.Array(sorted(p for p in self.items if p.startswith("/legacy/"+leaf+"/")),signature="o",variant_level=1)
        if interface==ITEM:
            data=self.items[path]
            if name=="Label": return dbus.String(data[0],variant_level=1)
            if name=="Attributes": return dbus.Dictionary({str(n):"value" for n in range(33)} if self.mode=="attributes" else data[1],signature="ss",variant_level=1)
        raise RuntimeError("Invalid private property")
    def forge(self,request,args,signature):
        reply=dbus.lowlevel.MethodReturnMessage(request);reply.append(*args,signature=signature);self.attacker.send_message(reply)
    def close(self):
        for obj in self.objects: obj.remove_from_connection()
        for bus in (self.ss,self.kw,self.attacker,self.replacement): bus.close()
