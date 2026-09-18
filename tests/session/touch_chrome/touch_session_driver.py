#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""KWin --exit-with-session program for the touch-chrome rows (ADR-0193).

Started by run_touch_chrome.py through qindaqt-wm inside a private virtual
session; configuration arrives through the SHADE_* environment variables the
shade harness defined, because this driver is that harness driving fingers.
"""

from __future__ import annotations

import sys
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "shade_visibility"))

import touch_flow_chrome  # noqa: E402
from shade_session import SessionConfig, ShadeSession  # noqa: E402

FLOWS = {"chrome": touch_flow_chrome.run}


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
