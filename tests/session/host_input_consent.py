# SPDX-License-Identifier: GPL-3.0-or-later
"""Require explicit per-invocation consent before creating host uinput devices."""

from __future__ import annotations

from collections.abc import Mapping


HOST_UINPUT_CONSENT_ENV = "QINDAQT_ALLOW_HOST_UINPUT"
HOST_UINPUT_CONSENT_VALUE = "I_UNDERSTAND_THIS_CAN_CONTROL_THE_HOST_DESKTOP"
HOST_UINPUT_SKIP_CODE = 77


def host_uinput_consent_error(
    requested: bool, environment: Mapping[str, str]
) -> str | None:
    """Return a diagnostic unless a requested host-input run has exact consent."""
    if not requested:
        return None
    if environment.get(HOST_UINPUT_CONSENT_ENV) == HOST_UINPUT_CONSENT_VALUE:
        return None
    return (
        "host uinput workflow skipped: it can move, click, and type on the active "
        f"desktop; use a dedicated virtual seat and set {HOST_UINPUT_CONSENT_ENV}="
        f"{HOST_UINPUT_CONSENT_VALUE} for this invocation"
    )
