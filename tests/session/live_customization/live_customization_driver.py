#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""KWin --exit-with-session program for the live-customization rows.

Started by run_live_customization.py through qindaqt-wm inside a private
virtual session; configuration arrives through LC_* environment variables.
Writes evidence.json (steps, verdicts, captures) and exits nonzero only when
the flow itself could not complete. The runner, not this driver, decides
pass/fail.
"""

from __future__ import annotations

import sys
import traceback
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "shade_visibility"))

from live_customization_flows import FLOWS  # noqa: E402
from live_customization_session import LiveSession, SessionConfig  # noqa: E402


def main() -> int:
    config = SessionConfig.from_environment()
    config.output.mkdir(parents=True, exist_ok=True)
    session = LiveSession(config)
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
