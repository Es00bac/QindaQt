#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic private-bus fixture only; never read user data or accept passwords in argv."""
import sys
import time
if "delayed-fixture" in sys.argv:
    time.sleep(1.2)
sys.stdout.buffer.write(b"synthetic-keyring-password")
