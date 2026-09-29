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
fd=int(os.environ["WAYLAND_SOCKET"])
peer=socket.socket(fileno=fd)
pid,uid,gid=struct.unpack("3i",peer.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12))
path=pathlib.Path(os.environ["XDG_RUNTIME_DIR"])/"prompt-metadata"
path.write_text(json.dumps({"display":os.environ["WAYLAND_DISPLAY"],"pid":pid,"uid":uid}))
if "delayed-fixture" in sys.argv: time.sleep(2)
sys.stdout.buffer.write(b"synthetic-keyring-password");sys.stdout.flush()
