#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fixed synthetic credentials, never caller-provided password arguments."""
import sys
frame=sys.stdin.buffer.read(1289)
assert 8<=len(frame)<=1288 and frame[:4]==b"QMP1"
label_size=int.from_bytes(frame[4:6],"big")
caller_size=int.from_bytes(frame[6:8],"big")
assert label_size<=1024 and caller_size<=256 and len(frame)==8+label_size+caller_size
label=frame[8:8+label_size].decode("utf-8")
action=sys.argv[sys.argv.index("--action")+1]
old=b"synthetic-keyring-password"
if action=="reveal" and label=="wrong-auth-fixture": old=b"incorrect"
if action=="confirm-delete": output=b"QKOK"
elif action=="change-password":
    new=b"synthetic-new-password"
    if label=="wrong-auth-fixture": old=b"incorrect"
    if label=="malformed-frame-fixture": output=b"QKP1"+bytes([16,1,0,0])
    else: output=b"QKP1"+len(old).to_bytes(2,"big")+len(new).to_bytes(2,"big")+old+new
else: output=old
sys.stdout.buffer.write(output)
