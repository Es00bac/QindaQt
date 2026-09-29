#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private test compositor metadata/socket peer; no Wayland protocol rendering."""
import os
import pathlib
import select
import socket
import sys
import secretstorage
from jeepney import DBusAddress,new_method_call
bus=secretstorage.dbus_init()
address=DBusAddress("/org/freedesktop/DBus","org.freedesktop.DBus","org.freedesktop.DBus")
bus.send_and_get_reply(new_method_call(address,"RequestName","su",("org.qindaqt.KWin",4)))
path=pathlib.Path(os.environ["XDG_RUNTIME_DIR"])/sys.argv[1]
listener=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
listener.bind(str(path));os.chmod(path,0o600);listener.listen(8)
peers=[]
print("ready",flush=True)
while True:
    ready,_,_=select.select([listener,sys.stdin],[],[])
    if listener in ready:
        peer,_=listener.accept();peers.append(peer)
    if sys.stdin in ready:
        command=sys.stdin.readline().strip()
        if command=="release":
            bus.send_and_get_reply(new_method_call(address,"ReleaseName","s",("org.qindaqt.KWin",)))
        elif command=="drop":
            for peer in peers: peer.close()
            peers.clear()
        elif command=="unlink":
            path.unlink()
        elif command=="quit" or not command: break
        print("done",flush=True)
for peer in peers: peer.close()
listener.close();bus.close()
