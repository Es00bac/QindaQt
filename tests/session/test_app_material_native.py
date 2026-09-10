#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Render the real first-party views on private native Wayland outputs.

This supplements the full desktop profile/container matrix. It proves native
client rendering and fixture input, not physical input or compositor grouping.
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time
from nested_session_scenario import (isolated_environment, running_private_session_bus,
                                     VirtualOutputSpec, write_virtual_output_config)


def process_identity(pid: int, runtime: str) -> str | None:
    """Return Linux starttime only for this UID and exact disposable runtime."""
    directory = Path('/proc') / str(pid)
    try:
        if pid == os.getpid() or directory.stat().st_uid != os.getuid():
            return None
        fields = (directory / 'stat').read_text().rsplit(')', 1)[1].split()
        if fields[0] == 'Z':
            return None  # Exited zombies cannot hold our sockets or execute.
        marker = os.fsencode('XDG_RUNTIME_DIR=' + runtime)
        if marker not in (directory / 'environ').read_bytes().split(b'\0'):
            return None
        return fields[19]  # Linux stat field 22, relative to state (field 3).
    except (FileNotFoundError, ProcessLookupError, PermissionError):
        return None


def owned_processes(runtime: str) -> dict[int, str]:
    result = {}
    for entry in Path('/proc').iterdir():
        if entry.name.isdecimal():
            pid = int(entry.name)
            identity = process_identity(pid, runtime)
            if identity is not None:
                result[pid] = identity
    return result


def signal_owned(runtime: str, processes: dict[int, str], number: int) -> None:
    for pid, identity in processes.items():
        try:
            # AGENT-GUARD: A PID may be recycled between discovery and signaling.
            # Pin its kernel identity, then recheck UID/starttime/private runtime;
            # never signal a bare PID found by name or a host-session process.
            descriptor = os.pidfd_open(pid)
            try:
                if process_identity(pid, runtime) == identity:
                    signal.pidfd_send_signal(descriptor, number)
            finally:
                os.close(descriptor)
        except ProcessLookupError:
            pass


def retire_private_processes(runtime: str) -> None:
    # PTY shells use setsid(), so process-group cleanup alone misses descendants.
    # This unique per-row environment marker follows them across session changes.
    for number, grace in ((signal.SIGTERM, 1.0), (signal.SIGKILL, 2.0)):
        deadline = time.monotonic() + grace
        while True:
            remaining = owned_processes(runtime)
            if not remaining:
                return
            signal_owned(runtime, remaining, number)
            if time.monotonic() >= deadline:
                break
            time.sleep(.025)
    remaining = owned_processes(runtime)
    if remaining:
        raise RuntimeError(f'Private native processes survived teardown: {sorted(remaining)}')


@contextmanager
def private_process_cleanup(environment: dict[str, str]):
    try:
        yield
    finally:
        retire_private_processes(environment['XDG_RUNTIME_DIR'])


def stop_process(process: subprocess.Popen) -> None:
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=1)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=2)


def run(command: list[str], environment: dict[str, str], log: Path, timeout: int = 60) -> None:
    with log.open('w') as output:
        process = subprocess.Popen(command, env=environment, stdin=subprocess.DEVNULL,
                                   stdout=output, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        try:
            result = process.wait(timeout=timeout)
        finally:
            stop_process(process)
    if result:
        raise RuntimeError(f'{Path(command[0]).name} exited {result}; see {log}')


def interrupted(signum, frame) -> None:
    # Let CTest's TERM unwind all context managers; a second TERM must not
    # interrupt the bounded teardown. SIGKILL cannot be handled by any runner.
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    raise InterruptedError('Native app matrix terminated')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--build-root', type=Path, required=True)
    arguments = parser.parse_args()
    source, build = arguments.source_root.resolve(), arguments.build_root.resolve()
    kwin, dbus = shutil.which('kwin_wayland'), shutil.which('dbus-daemon')
    doctor = shutil.which('kscreen-doctor')
    if not kwin or not dbus or not doctor:
        print('Native app matrix requires KWin, dbus-daemon and kscreen-doctor')
        return 77
    artifacts = build / 'tests/session/app-material-native'
    artifacts.mkdir(parents=True, exist_ok=True)
    (artifacts/'evidence.json').unlink(missing_ok=True)
    scratch = source / '.cache'
    scratch.mkdir(exist_ok=True)
    evidence = []
    # CLI extents are logical. These rows have exact integral logical sizes.
    for pixel_width, pixel_height, scale in ((1920,1080,1.0), (1920,1200,1.0),
                                             (2560,1440,1.25), (1920,1080,1.5)):
        row = artifacts / f'{pixel_width}x{pixel_height}-{scale:g}'
        if row.exists(): shutil.rmtree(row)
        row.mkdir()
        with tempfile.TemporaryDirectory(prefix='n-', dir=scratch) as temporary:
            root = Path(temporary)
            environment = isolated_environment(root)
            for key in ('QT_SCALE_FACTOR', 'QT_SCREEN_SCALE_FACTORS',
                        'QT_AUTO_SCREEN_SCALE_FACTOR', 'QT_ENABLE_HIGHDPI_SCALING',
                        'QT_FONT_DPI', 'QT_DEVICE_PIXEL_RATIO',
                        'QT_SCALE_FACTOR_ROUNDING_POLICY', 'QT_USE_PHYSICAL_DPI'):
                environment.pop(key, None)
            environment['TMPDIR'] = str(root / 'cache')
            environment['QML_IMPORT_PATH'] = str(build / 'qml')
            environment['QML2_IMPORT_PATH'] = str(build / 'qml')
            environment['QT_FATAL_WARNINGS'] = '1'
            environment['WAYLAND_DISPLAY'] = 'qq-material'
            spec = VirtualOutputSpec('app-material', 1, pixel_width, pixel_height,
                    round(pixel_width/scale), round(pixel_height/scale), scale)
            write_virtual_output_config(root/'config', spec)
            command = [kwin, '--virtual', '--width', str(round(pixel_width/scale)),
                       '--height', str(round(pixel_height/scale)), '--scale', str(scale),
                       '--output-count', '1', '--socket', 'qq-material',
                       '--no-lockscreen', '--no-global-shortcuts']
            with private_process_cleanup(environment), \
                    running_private_session_bus(root, Path(dbus), environment):
                with (row / 'kwin.log').open('w') as output:
                    compositor_env = dict(environment)
                    compositor_env['QT_QPA_PLATFORM'] = 'offscreen'
                    # The client warning gate remains strict. A virtual KWin
                    # without resident session services legitimately logs their
                    # absence and is checked by socket/process readiness.
                    compositor_env.pop('QT_FATAL_WARNINGS', None)
                    compositor = subprocess.Popen(command, env=compositor_env,
                        stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
                    try:
                        socket = root / 'runtime/qq-material'
                        deadline = time.monotonic()+10
                        while not socket.exists():
                            if compositor.poll() is not None or time.monotonic()>deadline:
                                raise RuntimeError(f'Private KWin not ready; see {row / "kwin.log"}')
                            time.sleep(.025)
                        topology_env = dict(environment)
                        topology_env['KSCREEN_BACKEND'] = 'KWayland'
                        topology_env['KSCREEN_BACKEND_INPROCESS'] = '1'
                        topology = subprocess.run([doctor, '-j'], env=topology_env,
                            capture_output=True, text=True, timeout=10)
                        (row/'outputs.json').write_text(topology.stdout)
                        if topology.returncode:
                            raise RuntimeError(f'Cannot observe native outputs: {topology.stderr}')
                        outputs = json.loads(topology.stdout)['outputs']
                        enabled = [item for item in outputs if item.get('enabled')]
                        if len(enabled) != 1 or abs(enabled[0]['scale']-scale) > .001:
                            raise RuntimeError(f'Observed output scale differs: {topology.stdout}')
                        mode = next(item for item in enabled[0]['modes']
                                    if item['id'] == enabled[0]['currentModeId'])
                        if mode['size'] != {'width':pixel_width,'height':pixel_height}:
                            raise RuntimeError(f'Observed pixel mode differs: {topology.stdout}')
                        for theme in ('qinda-light','qinda-dark','qinda-high-contrast'):
                            run([str(build/'tests/apps/file_manager/qindaqt_file_manager_visual_probe'),
                                 str(source),theme,str(row/f'files-{theme}.png'),'900','600'],
                                environment,row/f'files-{theme}.log')
                        run([str(build/'tests/apps/file_manager/qindaqt_file_manager_visual_probe'),
                             str(source),'qinda-dark',str(row/'files-compact.png'),'480','320'],
                            environment,row/'files-compact.log')
                        editor_captures = build/'tests/apps/text_editor/captures'
                        if editor_captures.exists():
                            shutil.rmtree(editor_captures)
                        run([str(build/'tests/apps/text_editor/qindaqt_editor_visual_tests')],
                            environment,row/'editor.log')
                        expected = {f'{theme}-{size}{suffix}.png'
                                    for theme in ('dark', 'light', 'high-contrast')
                                    for size in ('wide', 'compact')
                                    for suffix in ('', '-search')}
                        actual = {path.name for path in editor_captures.glob('*.png')}
                        if actual != expected or any(
                                (editor_captures/name).stat().st_size == 0 for name in expected):
                            raise RuntimeError(f'Editor capture set differs: {sorted(actual)}')
                        shutil.copytree(editor_captures,row/'editor')
                        terminal_env = dict(environment)
                        terminal_env['QINDAQT_TEST_WINDOW_CAPTURE'] = str(row/'terminal')
                        run([str(build/'tests/apps/terminal/qindaqt_terminal_widget_adapter_tests'),
                             'productionWindowPaintsInteractivePrompt'],terminal_env,row/'terminal.log')
                        evidence.append({'pixels':[pixel_width,pixel_height], 'scale':scale,
                                         'observedOutput':enabled[0],
                                         'artifacts':str(row.relative_to(build)), 'result':'passed'})
                    finally:
                        stop_process(compositor)
        print(f'Native apps {pixel_width}x{pixel_height} at {scale:g}: passed', flush=True)
    (artifacts/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    return 0

if __name__ == '__main__':
    signal.signal(signal.SIGTERM, interrupted)
    raise SystemExit(main())
