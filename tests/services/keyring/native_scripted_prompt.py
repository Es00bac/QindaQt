#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fixed synthetic credentials, never caller-provided password arguments."""
import sys
import time
from prompt_metadata_fixture import read_metadata
metadata = read_metadata()
label = metadata["label"]
action=sys.argv[sys.argv.index("--action")+1]
if action=="reveal" and label=="delayed-reveal-fixture": time.sleep(1.0)
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
