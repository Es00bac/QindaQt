# SPDX-License-Identifier: GPL-3.0-or-later
"""Drive a real org.qindaqt.Voice1 provider from QindaQt's own client.

This is the cross-implementation row for the voice contract: Gabbee's actual
provider is published on a private session bus, and QindaQt's compiled
``qindaqt_voice_probe`` binary is what talks to it. Neither side is stubbed, so
a key rename, a type change or a broken admission rule fails here rather than
in a live session.

Nothing about this probe touches a microphone, an audio device, or a speech
provider: the Gabbee controller it publishes is a fixed fake whose state the
probe drives directly. There is no recording and no transcription.

Exit codes: 0 pass, 1 fail, 77 skip (the Gabbee checkout or its interpreter is
absent, or no ``dbus-run-session`` exists on this host).
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SKIP = 77

DEFAULT_GABBEE = Path.home() / "gabbee"


def _skip(reason: str) -> int:
    print(f"SKIP: {reason}")
    return SKIP


def _gabbee_interpreter(checkout: Path) -> Path | None:
    """Gabbee's own virtualenv, which is where PyQt6 is guaranteed to be."""

    candidate = checkout / ".venv" / "bin" / "python"
    if candidate.exists():
        return candidate
    return None


def build_publisher_source() -> str:
    """The in-bus half: publish Gabbee's provider over a scripted fake.

    Written out rather than imported so the probe stays a single file that can
    be copied to a machine that has Gabbee but not this checkout.
    """

    return '''
import json, sys, threading
from PyQt6.QtCore import QCoreApplication, QTimer
from gabbee.models import ControllerSnapshot, ControllerState
from gabbee.qindaqt_voice import QindaQtVoiceService


class Config:
    stt_provider = "mock"
    language_code = "en"
    audio_source = None
    toggle_shortcut = "F5"
    command_shortcut = "F6"
    elevenlabs_realtime_enabled = False
    secret_api_key = None
    gemini_api_key = None
    env_values = {}

    def __init__(self):
        self.saved = {}

    def save(self, updates):
        self.saved.update(updates)


class Recorder:
    level_observer = None


class Controller:
    """A controller that records calls and never touches a microphone."""

    def __init__(self):
        self.input_enabled = True
        self.recorder = Recorder()
        self.calls = []
        self.state = ControllerState.IDLE
        self.last_text = ""
        self.partial_text = ""
        self.listeners = []

    def snapshot(self):
        return ControllerSnapshot(
            state=self.state,
            provider="mock",
            delivery_method="ibus" if self.last_text else "",
            last_text=self.last_text,
            error_message="",
            partial_text=self.partial_text,
            command_mode=False,
        )

    def add_listener(self, listener):
        self.listeners.append(listener)

    def notify(self):
        for listener in list(self.listeners):
            listener(self.snapshot())

    def start(self):
        self.calls.append("start")
        self.state = ControllerState.RECORDING
        self.partial_text = "the quick"
        self.notify()

    def start_command(self):
        self.calls.append("start_command")

    def stop(self):
        self.calls.append("stop")
        self.state = ControllerState.IDLE
        self.partial_text = ""
        self.last_text = "the quick brown fox"
        self.notify()

    def cancel(self):
        self.calls.append("cancel")
        self.state = ControllerState.IDLE
        self.partial_text = ""
        self.notify()

    def retry_last(self):
        self.calls.append("retry_last")
        return True

    def reload_transcriber(self):
        self.calls.append("reload_transcriber")

    def set_input_enabled(self, enabled):
        self.calls.append("set_input_enabled:%s" % bool(enabled))
        self.input_enabled = bool(enabled)
        self.notify()


def main():
    app = QCoreApplication(sys.argv)
    controller = Controller()
    service = QindaQtVoiceService(controller, Config())
    if not service.open():
        print(json.dumps({"published": False}), flush=True)
        return 1
    # Tell the driver the bus is ready, then run until it is finished with us.
    print(json.dumps({"published": True}), flush=True)

    def watch_stdin():
        sys.stdin.readline()
        QTimer.singleShot(0, app.quit)

    threading.Thread(target=watch_stdin, daemon=True).start()
    return app.exec()


sys.exit(main())
'''


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gabbee", type=Path, default=DEFAULT_GABBEE,
                        help="Gabbee checkout to publish (default: ~/gabbee)")
    parser.add_argument("--probe", type=Path, required=True,
                        help="Path to the compiled qindaqt_voice_probe binary")
    arguments = parser.parse_args(argv)

    if shutil.which("dbus-run-session") is None:
        return _skip("dbus-run-session is not installed")
    checkout = arguments.gabbee
    provider = checkout / "src" / "gabbee" / "qindaqt_voice.py"
    if not provider.exists():
        return _skip(f"no Gabbee provider at {provider}")
    interpreter = _gabbee_interpreter(checkout)
    if interpreter is None:
        return _skip(f"no Gabbee virtualenv interpreter under {checkout}")
    if not arguments.probe.exists():
        return _skip(f"no voice probe binary at {arguments.probe}")

    # AGENT-GUARD: the publisher script goes to a temporary directory, never
    # into the provider's checkout. A probe must not put a stray untracked file
    # in someone's working tree, least of all one it might fail to clean up.
    workspace = tempfile.mkdtemp(prefix="qindaqt-voice-interop-")
    publisher = pathlib.Path(workspace) / "publisher.py"
    publisher.write_text(build_publisher_source())
    environment = dict(os.environ)
    environment["PYTHONPATH"] = str(checkout / "src")
    environment["QT_QPA_PLATFORM"] = "offscreen"
    environment["PYTHONDONTWRITEBYTECODE"] = "1"

    # One private bus hosts both halves. dbus-run-session runs the shell below,
    # which starts the provider, waits for it to own the name, then runs the
    # compiled client against it.
    driver = (
        f'{interpreter} {publisher} & '
        'PUBLISHER=$!; '
        'for _ in $(seq 1 100); do '
        '  dbus-send --session --dest=org.freedesktop.DBus '
        '    --type=method_call --print-reply /org/freedesktop/DBus '
        '    org.freedesktop.DBus.GetNameOwner string:org.qindaqt.Voice1 '
        '    >/dev/null 2>&1 && break; '
        '  sleep 0.1; '
        'done; '
        f'{arguments.probe}; '
        'STATUS=$?; '
        'kill $PUBLISHER 2>/dev/null; '
        'wait $PUBLISHER 2>/dev/null; '
        'exit $STATUS'
    )
    try:
        completed = subprocess.run(
            ["dbus-run-session", "--", "sh", "-c", driver],
            env=environment, timeout=120, text=True, capture_output=True)
    except subprocess.TimeoutExpired:
        print("FAIL: the interop run did not finish within 120 seconds")
        return 1
    finally:
        shutil.rmtree(workspace, ignore_errors=True)

    sys.stdout.write(completed.stdout)
    if completed.returncode != 0:
        sys.stderr.write(completed.stderr)
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
