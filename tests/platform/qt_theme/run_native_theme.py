#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the real Qt theme plugin on a private activation-free bus and XDG tree."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="qindaqt-native-theme-") as name:
    root = Path(name)
    for child in ("runtime", "config", "data", "cache"):
        (root / child).mkdir(mode=0o700)
    shutil.copytree(Path(sys.argv[3]) / "data/themes", root / "data/qindaqt/themes")
    config = root / "bus.conf"
    config.write_text(f'''<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig><type>session</type><listen>unix:path={root}/bus</listen><auth>EXTERNAL</auth><policy context="default"><allow send_destination="*"/><allow receive_sender="*"/><allow own="*"/></policy></busconfig>''')
    bus = subprocess.Popen(["dbus-daemon", "--nofork", "--print-address=1", f"--config-file={config}"], stdout=subprocess.PIPE, text=True)
    try:
        address = bus.stdout.readline().strip()
        if not address:
            raise RuntimeError("private bus did not publish its address")
        env = dict(os.environ)
        for key in ("DISPLAY", "WAYLAND_DISPLAY", "QT_STYLE_OVERRIDE", "QML_IMPORT_PATH", "QML2_IMPORT_PATH"):
            env.pop(key, None)
        env.update(DBUS_SESSION_BUS_ADDRESS=address, QT_QPA_PLATFORM="offscreen", QT_QPA_PLATFORMTHEME="qindaqt",
                   QT_QUICK_CONTROLS_STYLE="Fusion", QT_QUICK_BACKEND="software", QT_FATAL_WARNINGS="1",
                   QT_PLUGIN_PATH=sys.argv[2], XDG_RUNTIME_DIR=str(root / "runtime"), XDG_CONFIG_HOME=str(root / "config"),
                   XDG_DATA_HOME=str(root / "data"), XDG_CACHE_HOME=str(root / "cache"), XDG_DATA_DIRS=str(root / "data"))
        if len(sys.argv) > 4:
            env["QT_STYLE_OVERRIDE"] = sys.argv[4]
        result = subprocess.run([sys.argv[1]], env=env, timeout=30, check=False)
        sys.exit(result.returncode)
    finally:
        bus.terminate()
        bus.wait(timeout=5)
