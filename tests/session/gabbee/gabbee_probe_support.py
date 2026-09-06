# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared helpers for the Gabbee/QindaQt interoperability probes.

Everything in this module is deliberately host-safe: no microphone capture,
no uinput device, no synthetic input events, and no connection to the user's
real session bus or Wayland display.  The dictation probe replaces Gabbee's
recorder with :class:`SyntheticRecorder`, which writes a fixed silent WAV
file, and Gabbee's mock STT provider turns that file into one fixed harmless
transcript.

AGENT-CONTRACT: The Gabbee checkout at GABBEE_SOURCE_ROOT is read-only inputs.
Nothing here writes into it.  QindaQt production files are likewise never
mutated; the portal configuration is staged into a private run root.
"""

from __future__ import annotations

import json
import os
import struct
import wave
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

# Exact defaults for this host lane; overridable for other machines.
DEFAULT_GABBEE_SOURCE_ROOT = Path("/home/cabewse/gabbee")
DEFAULT_GABBEE_VENV_PYTHON = DEFAULT_GABBEE_SOURCE_ROOT / ".venv/bin/python"

SYNTHETIC_RECORDING_NAME = "gabbee-synthetic.wav"
SYNTHETIC_SAMPLE_RATE = 16_000
SYNTHETIC_SECONDS = 0.25

RESULT_SCHEMA_VERSION = 1

PORTAL_INTERFACE_LINE = "org.freedesktop.impl.portal.GlobalShortcuts=kde"
PORTALS_CONF_NAME = "portals.conf"
FAKE_BACKEND_BUS_NAME = "org.freedesktop.impl.portal.desktop.kde"
FAKE_CONTROL_BUS_NAME = "org.qindaqt.test.GabbeePortalFake"
FRONTEND_BUS_NAME = "org.freedesktop.portal.Desktop"

# Forbidden environment keys mirror the session sandbox contract
# (tests/session/desktop_session_sandbox.py) plus the uinput acknowledgement
# that must never leak into a Gabbee interop run.
PROBE_FORBIDDEN_ENVIRONMENT = frozenset(
    {
        "DISPLAY",
        "WAYLAND_DISPLAY",
        "WAYLAND_SOCKET",
        "XAUTHORITY",
        "DBUS_STARTER_ADDRESS",
        "DBUS_STARTER_BUS_TYPE",
        "PIPEWIRE_REMOTE",
        "PULSE_SERVER",
        "QINDAQT_DOTOOL",
        "QINDAQT_ALLOW_HOST_UINPUT",
        "QINDAQT_ENABLE_HOST_UINPUT_TESTS",
    }
)


class ProbeContractError(RuntimeError):
    """Raised when the interop probe's own preconditions are violated."""


def gabbee_source_root(explicit: str | os.PathLike[str] | None = None) -> Path:
    root = Path(explicit or os.environ.get("GABBEE_SOURCE_ROOT") or DEFAULT_GABBEE_SOURCE_ROOT)
    if not (root / "src/gabbee/controller.py").is_file():
        raise ProbeContractError(f"Gabbee source root is unusable: {root}")
    return root


def gabbee_venv_python(explicit: str | os.PathLike[str] | None = None) -> Path:
    candidate = Path(
        explicit or os.environ.get("GABBEE_VENV_PYTHON") or DEFAULT_GABBEE_VENV_PYTHON
    )
    if not candidate.is_file():
        raise ProbeContractError(f"Gabbee venv interpreter is missing: {candidate}")
    return candidate


def synthetic_transcript() -> str:
    """The exact transcript Gabbee's mock STT produces for the synthetic WAV.

    Mirrors gabbee.stt.mock.MockSpeechToText without importing it so this
    helper stays usable on machines without the Gabbee checkout.
    """

    return f"[mock transcript from {SYNTHETIC_RECORDING_NAME}]"


class SyntheticRecorder:
    """Drop-in replacement for Gabbee's PipeWireRecorder.

    AGENT-GUARD: This recorder never opens an audio device.  It writes one
    fixed, bounded, silent PCM WAV so the probe satisfies Gabbee's
    RecorderProtocol without microphone capture of any kind.
    """

    def __init__(
        self,
        *,
        sample_rate: int = SYNTHETIC_SAMPLE_RATE,
        seconds: float = SYNTHETIC_SECONDS,
    ) -> None:
        self._sample_rate = sample_rate
        self._seconds = seconds
        self._path: Path | None = None
        self._recording = False

    @property
    def fixed_pcm_frames(self) -> int:
        return max(1, int(self._sample_rate * self._seconds))

    def is_recording(self) -> bool:
        return self._recording

    def start(self, output_path: Path, source_name: str | None = None) -> None:
        if self._recording:
            raise ProbeContractError("synthetic recorder is already recording")
        output_path = Path(output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        frames = self.fixed_pcm_frames
        with wave.open(str(output_path), "wb") as handle:
            handle.setnchannels(1)
            handle.setsampwidth(2)
            handle.setframerate(self._sample_rate)
            handle.writeframes(b"\x00\x00" * frames)
        self._path = output_path
        self._recording = True

    def stop(self) -> Path:
        if not self._recording or self._path is None:
            raise ProbeContractError("synthetic recorder is not recording")
        self._recording = False
        return self._path

    def cancel(self) -> None:
        self._recording = False
        self._path = None


def validate_synthetic_wav(path: Path) -> dict[str, Any]:
    """Return header facts for the synthetic WAV; raise on any deviation."""

    with wave.open(str(path), "rb") as handle:
        facts = {
            "channels": handle.getnchannels(),
            "sampleWidth": handle.getsampwidth(),
            "frameRate": handle.getframerate(),
            "frames": handle.getnframes(),
        }
        if facts != {
            "channels": 1,
            "sampleWidth": 2,
            "frameRate": SYNTHETIC_SAMPLE_RATE,
            "frames": SyntheticRecorder().fixed_pcm_frames,
        }:
            raise ProbeContractError(f"synthetic WAV deviates from the fixed contract: {facts}")
        pcm = handle.readframes(handle.getnframes())
    if pcm != b"\x00\x00" * (len(pcm) // 2):
        raise ProbeContractError("synthetic WAV contains non-silent samples")
    return facts


def stage_portals_configuration(source_conf: Path, destination: Path) -> dict[str, Any]:
    """Stage a runtime portals.conf copy routing GlobalShortcuts to the kde backend.

    Mirrors the independently accepted portal candidate 8215a8cd (selector
    line ``org.freedesktop.impl.portal.GlobalShortcuts=kde``) without editing
    any production file: the copy lives in the probe's private run root.

    AGENT-GUARD: When the integrated tree already routes GlobalShortcuts the
    staging becomes a byte-identical passthrough so the probe can never widen
    routing beyond the reviewed contract.
    """

    text = source_conf.read_text(encoding="utf-8")
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    already = [line for line in lines if line.startswith("org.freedesktop.impl.portal.GlobalShortcuts=")]
    if already:
        if already != [PORTAL_INTERFACE_LINE]:
            raise ProbeContractError(
                f"integrated portal routing diverges from the reviewed kde-only line: {already}"
            )
        staged = text
        mode = "passthrough"
    else:
        if not lines or lines[0] != "[preferred]":
            raise ProbeContractError(f"unexpected portals.conf shape: {source_conf}")
        staged = text.rstrip("\n") + "\n" + PORTAL_INTERFACE_LINE + "\n"
        mode = "append-kde"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(staged, encoding="utf-8")
    staged_lines = [line.strip() for line in staged.splitlines() if line.strip()]
    global_shortcuts = [
        line
        for line in staged_lines
        if line.startswith("org.freedesktop.impl.portal.GlobalShortcuts=")
    ]
    if global_shortcuts != [PORTAL_INTERFACE_LINE]:
        raise ProbeContractError("staged portal configuration lost the GlobalShortcuts routing")
    return {
        "mode": mode,
        "source": str(source_conf),
        "destination": str(destination),
        "routing": global_shortcuts[0],
    }


@dataclass
class PhaseResult:
    phase: str
    ok: bool
    summary: str
    evidence: dict[str, Any] = field(default_factory=dict)


class ResultDocument:
    """Builds the probe's JSON evidence document with a stable schema."""

    def __init__(self, run_id: str, mode: str) -> None:
        self.document: dict[str, Any] = {
            "schemaVersion": RESULT_SCHEMA_VERSION,
            "runId": run_id,
            "mode": mode,
            "phases": [],
            "safety": {
                "microphoneCapture": False,
                "hostTyping": False,
                "hostInputInjection": False,
                "uinputDevicesCreated": False,
                "stt": "gabbee-mock (fixed harmless transcript)",
            },
        }

    def add(self, result: PhaseResult) -> None:
        self.document["phases"].append(
            {
                "phase": result.phase,
                "ok": result.ok,
                "summary": result.summary,
                "evidence": result.evidence,
            }
        )

    def outcome(self) -> bool:
        return bool(self.document["phases"]) and all(phase["ok"] for phase in self.document["phases"])

    def write(self, path: Path) -> None:
        self.document["ok"] = self.outcome()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(self.document, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    @staticmethod
    def load(path: Path) -> dict[str, Any]:
        document = json.loads(path.read_text(encoding="utf-8"))
        if document.get("schemaVersion") != RESULT_SCHEMA_VERSION:
            raise ProbeContractError(f"unsupported result schema: {path}")
        phases = document.get("phases")
        if not isinstance(phases, list) or not phases:
            raise ProbeContractError(f"result document has no phases: {path}")
        for phase in phases:
            if not {"phase", "ok", "summary", "evidence"} <= phase.keys():
                raise ProbeContractError(f"malformed phase entry: {phase}")
        if document.get("ok") != all(phase["ok"] for phase in phases):
            raise ProbeContractError(f"result ok flag disagrees with phases: {path}")
        return document


def probe_child_environment(base: dict[str, str], updates: dict[str, str]) -> dict[str, str]:
    """Merge environments for probe children while forbidding host endpoints."""

    merged = dict(base)
    merged.update(updates)
    overlap = PROBE_FORBIDDEN_ENVIRONMENT.intersection(merged)
    if overlap:
        raise ProbeContractError(f"probe environment admitted forbidden keys: {sorted(overlap)}")
    return merged


def gabbee_probe_python_environment(
    *, source_root: Path, venv_python: Path, system_site_packages: str
) -> dict[str, str]:
    """Environment for running real Gabbee modules under the venv interpreter.

    The Gabbee venv provides PyQt6 and dbus-python; the host/system
    site-packages provide PyGObject (gi) for Gabbee's AT-SPI backend and GLib
    main loop.  Both interpreters on this lane are CPython 3.14, so combining
    them is ABI-safe; on any other lane combination the probe fails its
    preflight instead of guessing.
    """

    parts = [str(path) for path in (Path(system_site_packages), source_root / "src")]
    return {
        "PYTHONPATH": os.pathsep.join(parts),
        "GABBEE_STT_PROVIDER": "mock",
        "GABBEE_AUDIO_SOURCE": "synthetic",
        "GABBEE_UI_TITLE": "Gabbee",
        # Pin the paths fixture under a private directory so Gabbee never
        # reads or writes configuration in a shared HOME.
        "PYTHONPYCACHEPREFIX": str(venv_python.parent / "__pycache__-probe"),
    }


def wav_header_facts(path: Path) -> dict[str, Any]:
    with open(path, "rb") as handle:
        header = handle.read(44)
    if len(header) != 44 or header[:4] != b"RIFF" or header[8:12] != b"WAVE":
        raise ProbeContractError(f"not a canonical RIFF/WAVE file: {path}")
    channels, rate = struct.unpack_from("<HI", header, 22)
    return {"channels": channels, "sampleRate": rate}


BUS_CONFIG_TEMPLATE = """<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>{address}</listen>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
  </policy>
  <servicedir>{service_dir}</servicedir>
</busconfig>
"""


def write_private_bus_config(run_root: Path) -> dict[str, Path]:
    """Create a private session-bus config whose ONLY service dir is ours.

    AGENT-GUARD: the stock session config activates backends from
    /usr/share/dbus-1/services; on a host with xdg-desktop-portal-kde
    installed, D-Bus activation would race the probe's fake KDE backend and
    silently hand it the reviewed selector's bus name.  A private config with
    one probe-owned service dir makes activation deterministic and keeps the
    chain off every host service, including at-spi and the real portals.
    """

    runtime = run_root / "runtime"
    service_dir = runtime / "services"
    service_dir.mkdir(parents=True, exist_ok=True)
    address = f"unix:path={runtime / 'bus'}"
    config_path = runtime / "bus.conf"
    config_path.write_text(
        BUS_CONFIG_TEMPLATE.format(address=address, service_dir=service_dir),
        encoding="utf-8",
    )
    return {"config": config_path, "address": address, "serviceDir": service_dir}


def write_fake_backend_service(
    service_dir: Path, *, venv_python: Path, fake_script: Path
) -> Path:
    """D-Bus service file activating the fake KDE backend under its own name."""

    service = service_dir / "org.freedesktop.impl.portal.desktop.kde.service"
    service.write_text(
        "[D-BUS Service]\n"
        f"Name={FAKE_BACKEND_BUS_NAME}\n"
        f"Exec={venv_python} {fake_script}\n",
        encoding="utf-8",
    )
    return service
