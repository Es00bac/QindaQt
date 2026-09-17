#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""KWin --exit-with-session program for the iconify-visibility rows (ADR-0191).

Started by run_iconify_visibility.py through qindaqt-wm inside a private
virtual session. Writes evidence.json (steps, verdicts, captures) and exits
nonzero only when the flow itself could not complete; the runner judges.
"""

from __future__ import annotations

import sys
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "shade_visibility"))
sys.path.insert(0, str(HERE))

import iconify_flow_basic  # noqa: E402
import iconify_flow_dock  # noqa: E402
from shade_session import SessionConfig, ShadeSession  # noqa: E402

FLOWS = {"basic": iconify_flow_basic.run, "dock": iconify_flow_dock.run}


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
