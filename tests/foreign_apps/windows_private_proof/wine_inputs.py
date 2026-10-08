# SPDX-License-Identifier: GPL-3.0-or-later
"""Fixed Wine inputs; no host auth or external runtime overrides."""
import os
from pathlib import Path
ROOT=Path("/fixture")
LOADER="/usr/lib/wine-proton-11.0.2/bin/wine"
SERVER="/usr/lib/wine-proton-11.0.2/bin/wineserver"
PE="/usr/lib/wine-proton-11.0.2/wine/x86_64-windows/"
# Each app receives only these fixed inputs plus nested display/auth created here.
def wine_environment(name):
    env={k:os.environ[k] for k in ["PATH","LANG","LC_ALL","DISPLAY","XDG_RUNTIME_DIR"] if k in os.environ}
    authority=os.environ.get("XAUTHORITY")
    # AGENT-GUARD: KWin's internal-Xwayland path exports exact empty authority.
    # Omit only unset/empty: Wine sees its own private HOME, never host auth.
    if authority is not None and authority!="":
        if authority.isspace():
            raise RuntimeError("Xauthority outside own sandbox")
        auth=Path(authority).resolve()
        if not (str(auth).startswith("/fixture/") or str(auth).startswith("/tmp/")):
            raise RuntimeError("Xauthority outside own sandbox")
        env["XAUTHORITY"]=str(auth)
    home=ROOT/name/"home"
    env.update(HOME=str(home),USER="fixture",LOGNAME="fixture",WINEPREFIX=str(ROOT/name/"prefix"),WINEARCH="win64",
               XDG_CONFIG_HOME=str(home/"config"),XDG_DATA_HOME=str(home/"data"),
               XDG_CACHE_HOME=str(home/"cache"),XDG_STATE_HOME=str(home/"state"),
               TMPDIR="/tmp",WINEDEBUG="-all",
               WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=d;winepulse.drv,winealsa.drv=d;winewayland.drv=d")
    return env

def authority_state():
    # Bounded original state only; never log auth pathname or cookie contents.
    value=os.environ.get("XAUTHORITY")
    return {"present":value is not None,"exactEmpty":value=="",
            "whitespaceOnly":bool(value) and value.isspace(),"cwd":os.getcwd()[:256]}

