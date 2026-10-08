#!/usr/bin/python3
"""Guest PID1 supervisor. Only the reviewed initramfs /init may enter this."""
import configparser
import json
import os
from pathlib import Path
import signal
import socket
import stat
import struct
import subprocess
import sys
import termios
import time

ROOT = Path("/run/android-proof")
children = []
audio_handles = []
cancelled = False

def cancel(*_):
    global cancelled
    cancelled = True

def check():
    if cancelled:
        raise RuntimeError("guest-cancelled")

def call(args, **kwargs):
    check()
    result = subprocess.run(args, stdin=subprocess.DEVNULL, timeout=20,
                            check=True, **kwargs)
    check()
    return result

def spawn(args, env=None, user=None):
    check()
    identity = {} if user is None else {"user": user, "group": user, "extra_groups": ()}
    child = subprocess.Popen(args, stdin=subprocess.DEVNULL, env=env, **identity)
    children.append(child)
    check()
    return child

def wait_path(path, timeout=10):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        check()
        if path.exists():
            return
        time.sleep(.05)
    raise RuntimeError("guest-readiness-timeout")

def process_start(pid):
    text = Path("/proc", str(pid), "stat").read_text()
    return text[text.rfind(")") + 2:].split()[19]

def owned_audio_socket(child, path, birth):
    """Only an endpoint of our held foreground child admits stock socket bind."""
    wait_path(path)
    before = path.lstat()
    if (child.poll() is not None or not stat.S_ISSOCK(before.st_mode)
            or before.st_uid != 1000):
        raise RuntimeError("private-audio-socket-admission")
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
        connection.settimeout(2)
        connection.connect(str(path))
        peer = struct.unpack("3i", connection.getsockopt(
            socket.SOL_SOCKET, socket.SO_PEERCRED, struct.calcsize("3i")))
    expected = os.stat("/usr/bin/pipewire")
    actual = os.stat("/proc/" + str(child.pid) + "/exe")
    after = path.lstat()
    if (peer != (child.pid, 1000, 1000) or process_start(child.pid) != birth
            or (actual.st_dev, actual.st_ino) != (expected.st_dev, expected.st_ino)
            or (before.st_dev, before.st_ino) != (after.st_dev, after.st_ino)
            or child.poll() is not None):
        raise RuntimeError("private-audio-owner-changed")

def start_private_audio(userenv, runtime):
    # AGENT-GUARD: guest-only stock compatibility endpoint, never host Audio1
    # integration. No session manager/device monitor or host socket is started.
    audioenv = dict(userenv, PIPEWIRE_CONFIG_DIR="/usr/share/pipewire",
                    PIPEWIRE_MODULE_DIR="/usr/lib64/pipewire-0.3",
                    SPA_PLUGIN_DIR="/usr/lib64/spa-0.2",
                    PIPEWIRE_RUNTIME_DIR=str(runtime), PIPEWIRE_CORE="pipewire-0",
                    PIPEWIRE_REMOTE="pipewire-0",
                    PULSE_RUNTIME_PATH=str(runtime / "pulse"))
    owned = []
    for name, endpoint in (("pipewire", runtime / "pipewire-0"),
                           ("pipewire-pulse", runtime / "pulse/native")):
        child = spawn(["/usr/bin/" + name], audioenv, user=1000)
        audio_handles.append(os.pidfd_open(child.pid))
        birth = process_start(child.pid)
        owned_audio_socket(child, endpoint, birth)
        owned.append(child)
    return owned

def bus_config(path):
    return ('<busconfig><type>session</type><listen>unix:path=' + path
            + '</listen><auth>EXTERNAL</auth><policy context="default">'
              '<allow user="*"/><allow own="*"/><allow send_destination="*"/>'
              '<allow receive_sender="*"/></policy></busconfig>')

def quarantine_terminal_failure():
    # AGENT-GUARD: A success line may already be queued. Exiting PID1 here can
    # give QEMU exit0 and falsely admit it. The unchanged host300s deadline
    # must terminate its owned QEMU and reject this run; never retry emission.
    while True:
        try:
            signal.pause()
        except BaseException:
            pass


def emit_terminal(result):
    fd = sys.stdout.fileno()
    if fd != 1 or not os.isatty(fd) or os.ttyname(fd) not in ("/dev/console", "/dev/ttyS0"):
        raise RuntimeError("terminal-not-guest-console")
    termios.tcgetattr(fd)  # Refuse an unsupported terminal before publication.
    os.sync()
    try:
        print("QINDA_ANDROID_RESULT=" + json.dumps(result, sort_keys=True), flush=True)
        # flush only drains Python. Wait for the guest serial driver before
        # returning from PID1, whose kernel panic otherwise interleaves bytes.
        termios.tcdrain(fd)
    except BaseException as error:
        try:
            print("QINDA_ANDROID_TERMINAL_ERROR=" + type(error).__name__[:64],
                  file=sys.stderr, flush=True)
        except BaseException:
            pass
        quarantine_terminal_failure()
        raise  # Injected test quarantine may return; production never does.


def main():
    if os.getpid() != 1 or Path("/proc/1/comm").read_text().strip() != "python3":
        raise RuntimeError("not-fixture-pid1")
    signal.signal(signal.SIGTERM, cancel)
    signal.signal(signal.SIGINT, cancel)
    # Stock privileged initialization creates configuration read by UID1000.
    # This conventional guest-only mask never changes the protected host stage.
    os.umask(0o022)
    result = {"schema": 1, "kind": "stock-android-feasibility",
              "success": False, "appIdentityQualified": False,
              "platformOriginQualified": False, "cleanupQualified": False,
              "audioIntegrationQualified": False, "privateAudioEndpointReady": False}
    env = {"PATH": "/usr/bin:/usr/sbin:/bin:/sbin", "LANG": "C.UTF-8",
           "HOME": "/root", "DBUS_SYSTEM_BUS_ADDRESS":
           "unix:path=/run/android-proof/system-bus",
           # Fixed qualified GCC runtime directory reproduces Gentoo's loader
           # cache inside this fixture; no host environment/cache is inherited.
           "LD_LIBRARY_PATH": "/usr/lib/gcc/x86_64-pc-linux-gnu/15"}
    os.environ.clear(); os.environ.update(env)
    Path("/etc/machine-id").write_text(Path("/proc/sys/kernel/random/uuid").read_text().replace("-", ""))
    phase = "guest-permissions"
    try:
        # The private host stage and generated usr/bin are0700. Only the
        # disposable guest copies need traversal by the fixed session UID.
        os.chmod("/", 0o755)
        os.chmod("/usr/bin", 0o755)
        ROOT.chmod(0o755)
        if Path("/etc/waydroid-extra/images").exists():
            raise RuntimeError("unexpected-image-override")
        if Path("/var/lib/waydroid").exists():
            raise RuntimeError("unexpected-existing-runtime")
        # Stock Waydroid creates /var/lib/waydroid with mkdir, not parents.
        # /var is a fresh guest-only tmpfs; no host directory is adopted.
        Path("/var/lib").mkdir(mode=0o755, exist_ok=True)
        # LXC compiled rootfs mountpoint is distinct from Waydroid image path.
        # The fresh /var tmpfs hides the package-owned empty mount directory.
        Path("/var/lib/lxc/rootfs").mkdir(parents=True, mode=0o755, exist_ok=True)
        phase = "private-buses"
        for name in ("system", "session"):
            conf = ROOT / (name + ".conf")
            conf.write_text(bus_config(str(ROOT / (name + "-bus"))))
            conf.chmod(0o644)
            spawn(["/usr/bin/dbus-daemon", "--nofork", "--config-file=" + str(conf)])
            wait_path(ROOT / (name + "-bus"))
        phase = "stock-init"
        call(["/usr/bin/waydroid", "--details-to-stdout", "init"])
        phase = "stock-config"
        config = configparser.ConfigParser()
        config.read("/var/lib/waydroid/waydroid.cfg")
        expected = {"images_path": "/usr/share/waydroid-extra/images",
                    "system_ota": "None", "vendor_ota": "None", "arch": "x86_64"}
        if any(config.get("waydroid", k, fallback=None) != v for k,v in expected.items()):
            raise RuntimeError("offline-image-admission")
        properties = Path("/var/lib/waydroid/waydroid_base.prop").read_text()
        if "waydroid.updater.disabled=true" not in properties:
            raise RuntimeError("updater-not-disabled")
        if "ro.hardware.egl=swiftshader" not in properties:
            raise RuntimeError("software-renderer-not-selected")
        phase = "stock-container"
        container = spawn(["/usr/bin/waydroid", "--details-to-stdout", "container", "start"])
        phase = "container-admission"
        import dbus
        bus = dbus.SystemBus()
        ready_end = time.monotonic() + 10
        while not bus.name_has_owner("id.waydro.Container"):
            check()
            if container.poll() is not None or time.monotonic() >= ready_end:
                raise RuntimeError("container-bus-readiness")
            time.sleep(.1)
        owner = str(bus.get_name_owner("id.waydro.Container"))
        bus_daemon = dbus.Interface(bus.get_object("org.freedesktop.DBus",
                                      "/org/freedesktop/DBus"), "org.freedesktop.DBus")
        if int(bus_daemon.GetConnectionUnixProcessID(owner)) != container.pid:
            raise RuntimeError("container-owner-not-held-process")
        phase = "user-paths"
        home = Path("/home/proof")
        home.mkdir(parents=True, mode=0o700); home.chmod(0o700)
        home.parent.chmod(0o755); os.chown(home, 1000, 1000)
        runtime = Path("/run/user/1000")
        runtime.mkdir(parents=True, mode=0o700); runtime.parent.chmod(0o755); os.chown(runtime, 1000, 1000)
        userenv = dict(env, HOME=str(home), USER="proof", LOGNAME="proof",
                       XDG_RUNTIME_DIR=str(runtime), QT_QPA_PLATFORM="offscreen",
                       QT_QUICK_BACKEND="software", LIBGL_ALWAYS_SOFTWARE="1",
                       KWIN_COMPOSE="Q", QT_NO_XDG_DESKTOP_PORTAL="1",
                       DBUS_SESSION_BUS_ADDRESS="unix:path=/run/android-proof/session-bus")
        command = ["/usr/bin/qindaqt-wm", "--kwin", "/usr/bin/qindaqt-kwin",
                   "--plugin-root", "/usr/lib64/qt6/plugins", "--virtual",
                   "--socket", "android-proof", "--width", "1280", "--height", "800",
                   "--scale", "1", "--output-count", "1", "--no-lockscreen",
                   "--no-global-shortcuts", "--test-scenario", "/proof/scenario.json",
                   "--session", "/proof/windows.py"]
        phase = "private-audio"
        audio = start_private_audio(userenv, runtime)
        if any(child.poll() is not None for child in audio):
            raise RuntimeError("private-audio-lost-before-session")
        result["privateAudioEndpointReady"] = True
        phase = "compositor-launch"
        compositor = spawn(command, userenv, user=1000)
        phase = "window-proof"
        compositor.wait(timeout=220)
        if compositor.returncode != 0:
            raise RuntimeError("nested-compositor-or-window-proof-failed")
        windows = json.loads((home / "windows.json").read_text())
        if windows.get("twoWindowsObserved") is not True:
            raise RuntimeError("two-window-proof-missing")
        if any(child.poll() is not None for child in audio):
            raise RuntimeError("private-audio-lost-during-session")
        result["windows"] = windows
        result["success"] = True
    except BaseException as error:
        result["errorType"] = type(error).__name__
        result["errorStage"] = phase
        number = getattr(error, "errno", None)
        result["errorErrno"] = number if isinstance(number, int) and 0 <= number <= 4095 else 0
        # Only this disposable public-input guest's exception is reported.
        result["errorMessage"] = str(error).replace("\n", " ").replace("\r", " ")[:256]
        result["success"] = False
    finally:
        # Entire namespace/kernel is fixture-owned, yet stop observation remains
        # distinct from QEMU containment. No cleanup is inferred from exit 0.
        try:
            call(["/usr/bin/waydroid", "--details-to-stdout", "container", "stop"])
            stopped = call(["/usr/bin/lxc-info", "-P", "/var/lib/waydroid/lxc",
                            "-n", "waydroid", "-sH"], capture_output=True, text=True)
            result["cleanupQualified"] = stopped.stdout.strip() == "STOPPED"
        except BaseException:
            result["cleanupQualified"] = False
        for child in reversed(children):
            try:
                if child.poll() is None:
                    child.terminate()
                child.wait(timeout=3)
            except BaseException:
                result["cleanupQualified"] = False
                # Direct unreaped Popen lifetime, never a discovered PID.
                try:
                    child.kill(); child.wait(timeout=2)
                except BaseException:
                    pass
        for handle in audio_handles:
            try: os.close(handle)
            except OSError: result["cleanupQualified"] = False
        result["success"] = result["success"] and result["cleanupQualified"]
        emit_terminal(result)
    return 0 if result["success"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
