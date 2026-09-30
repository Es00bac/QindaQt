#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Record only synthetic display metadata, never supplied passwords."""
import json
import os
import pathlib
import socket
import struct
import sys
import time
frame=sys.stdin.buffer.read(1289)
assert 8<=len(frame)<=1288 and frame[:4]==b"QMP1"
label_size=int.from_bytes(frame[4:6],"big")
caller_size=int.from_bytes(frame[6:8],"big")
assert label_size<=1024 and caller_size<=256 and len(frame)==8+label_size+caller_size
label=frame[8:8+label_size].decode("utf-8")
fd=int(os.environ["WAYLAND_SOCKET"])
peer=socket.socket(fileno=fd)
pid,uid,gid=struct.unpack("3i",peer.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12))
path=pathlib.Path(os.environ["XDG_RUNTIME_DIR"])/"prompt-metadata"
path.write_text(json.dumps({"display":os.environ["WAYLAND_DISPLAY"],"pid":pid,"uid":uid}))
if label=="delayed-fixture": time.sleep(2)
sys.stdout.buffer.write(b"synthetic-keyring-password");sys.stdout.flush()
