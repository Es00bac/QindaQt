#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic private-bus fixture only; never read user data or accept passwords in argv."""
import os
import json
import pathlib
import sys
import time
from prompt_metadata_fixture import read_metadata
metadata = read_metadata()
label = metadata["label"]
(pathlib.Path(os.environ["XDG_RUNTIME_DIR"])/"prompt-request.json").write_text(json.dumps(metadata))
with (pathlib.Path(os.environ["XDG_RUNTIME_DIR"])/"prompt-count").open("a") as count:
    count.write("prompt\n")
if label=="delayed-fixture":
    time.sleep(1.2)
sys.stdout.buffer.write(b"synthetic-keyring-password")
