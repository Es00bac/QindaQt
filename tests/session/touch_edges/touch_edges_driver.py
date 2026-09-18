#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""KWin --exit-with-session program for the touch edge gesture rows (ADR-0205).

Started by run_touch_edges.py through qindaqt-wm inside a private virtual
session; configuration arrives through the SHADE_* environment variables the
shade harness defined, because this driver is that harness swiping fingers
in from the output's edges.
"""

from __future__ import annotations

import sys
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "touch_chrome"))
sys.path.insert(0, str(HERE.parent / "shade_visibility"))

import touch_flow_edges  # noqa: E402
from shade_session import SessionConfig, ShadeSession  # noqa: E402

FLOWS = {"edges": touch_flow_edges.run}


def main() -> int:
    config = SessionConfig.from_environment()
    config.output.mkdir(parents=True, exist_ok=True)
    session = ShadeSession(config)
    status = 0
    try:
        session.start()
        FLOWS[config.flow](session)
        session.evidence["result"] = "completed"
    except Exception as error:  # noqa: BLE001 - recorded as evidence for the runner
        session.evidence["result"] = f"failed: {error}"
        session.evidence["traceback"] = traceback.format_exc()
        status = 1
    finally:
        session.write_evidence()
        session.close()
    return status


if __name__ == "__main__":
    raise SystemExit(main())
