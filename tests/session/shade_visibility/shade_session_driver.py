#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""KWin --exit-with-session program for the shade-visibility rows.

Started by run_shade_visibility.py through qindaqt-wm inside a private virtual
session; configuration arrives through SHADE_* environment variables. Writes
evidence.json (steps, verdicts, captures) and exits nonzero only when the flow
itself could not complete. The runner, not this driver, decides pass/fail.
"""

from __future__ import annotations

import sys
import traceback
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import shade_flow_cycles  # noqa: E402
import shade_flow_lifecycle  # noqa: E402
from shade_session import SessionConfig, ShadeSession  # noqa: E402

FLOWS = {"cycles": shade_flow_cycles.run, "lifecycle": shade_flow_lifecycle.run}


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
