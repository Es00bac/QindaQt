#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic private-bus fixture only; never read user data or accept passwords in argv."""
import sys
import time
frame=sys.stdin.buffer.read(1289)
assert 8<=len(frame)<=1288 and frame[:4]==b"QMP1"
label_size=int.from_bytes(frame[4:6],"big")
caller_size=int.from_bytes(frame[6:8],"big")
assert label_size<=1024 and caller_size<=256 and len(frame)==8+label_size+caller_size
label=frame[8:8+label_size].decode("utf-8")
if label=="delayed-fixture":
    time.sleep(1.2)
sys.stdout.buffer.write(b"synthetic-keyring-password")
