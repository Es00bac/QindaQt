#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real frontend/native input/frames in entirely private brokers and EGL display."""
import os
import json
import hashlib
import shutil
import pathlib
import resource
import select
import signal
import subprocess
import sys
import tempfile
import time
import stat

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
if len(sys.argv) not in (13, 15, 17):
    sys.exit("expected eleven artifacts, matching native plugin prefix and optional --case NAME / --renderer llvmpipe|render-node")
fixture, helper, compositor, consent, relay, exporter, pixels, metadata, selection, broker, qualified_support_program = map(pathlib.Path, sys.argv[1:12])
plugin_prefix = pathlib.Path(sys.argv[12]).resolve()
if plugin_prefix != compositor.resolve().parent or not (plugin_prefix / "qindaqt-kwin/plugins").is_dir():
    sys.exit("private native driver requires its matching built plugin prefix")
for artifact in (fixture, helper, compositor, consent, relay, exporter, pixels, metadata, selection, broker, qualified_support_program):
    if not artifact.is_file(): sys.exit("required native artifact unavailable: " + str(artifact))
    if artifact not in (metadata, selection) and not os.access(artifact, os.X_OK):
        sys.exit("required native executable unavailable: " + str(artifact))
if not compositor.is_file():
    print("qualified private compositor unavailable")
    sys.exit(77)
# Native Locked is irreversible without a trusted greeter. Each group gets a
# fresh private compositor; no unlock seam or host service is used.
groups = [
    ["screenshotActualPixelsColorAndCancel", "closeInvalidParentAndRequesterLoss",
     "validForeignParentGrantAndLossRetireCaptureAndStream", "actualPipeWireNodeFramesSessionCloseAndCancel", "frontendAndBrokerLossWithdrawCaptureAndFiles"],
    ["nativeLockStopsActualStreamPendingCaptureAndRetainedFile"],
    ["compositorLossWithdrawsStreamsFilesAndPendingPublication"],
    ["explicitTwoMonitorBatchClosesEveryProducer"],
    ["combinedRemoteDesktopSharesFramesClipboardAndCloses"],
    ["combinedNativeLockRetiresFramesAndInput"],
    ["explicitRememberRestoresProtectedSelection"],
]
# A bounded diagnostic selects the existing fixture's exact assertions.
# Input cases reuse this real broker/core setup with the input fixture artifact;
# they are not part of the capture fixture's all-case matrix.
input_cases = {"remoteDesktopCaptureAndClipboardJourney", "nativeLockEndsRemoteDesktop"}
# A selected case never means full capture or input coverage.
options = {}
for index in range(13, len(sys.argv), 2):
    key, value = sys.argv[index:index + 2]
    if key not in ("--case", "--renderer") or key in options:
        sys.exit("unknown or duplicate native capture diagnostic option")
    options[key] = value
renderer = options.get("--renderer", "llvmpipe")
if renderer not in ("llvmpipe", "render-node"):
    sys.exit("unknown native capture renderer")
if "--case" in options:
    if options["--case"] not in ({case for group in groups for case in group} | input_cases):
        sys.exit("unknown native capture diagnostic case")
    groups = [[options["--case"]]]
if renderer == "render-node":
    # AGENT-GUARD: this opt-in fixture mode permits allocation/rendering only.
    # A primary DRM card, input, sound, or writable sysfs is never a fallback.
    node = pathlib.Path("/dev/dri/renderD128")
    info = node.stat()
    if (not stat.S_ISCHR(info.st_mode) or (os.major(info.st_rdev), os.minor(info.st_rdev)) != (226, 128)
        or list(node.parent.iterdir()) != [node]
        or not os.statvfs("/sys").f_flag & os.ST_RDONLY
        or any(pathlib.Path(path).exists() for path in ("/dev/input", "/dev/snd"))
        or pathlib.Path("/sys/class/drm/renderD128/device/vendor").read_text().strip() != "0x1002"
        or pathlib.Path("/sys/class/drm/renderD128/device/device").read_text().strip() != "0x731f"):
        sys.exit("native render-node mode requires the isolated AMD1002:731f render-only device")


def group_alive(process):
    try:
        os.killpg(process.pid, 0)
        return True
    except ProcessLookupError:
        return False


def audit_exit(process):
    deadline = time.monotonic() + 2
    while group_alive(process) and time.monotonic() < deadline:
        time.sleep(.02)
    if group_alive(process):
        raise RuntimeError(f"private process group {process.pid} leaked descendants")


os.umask(0o077)
for group in groups:
    children = []
    logs = []
    with tempfile.TemporaryDirectory(prefix="qindaqt-native-capture-") as tmp:
        root = pathlib.Path(tmp)
        runtime = root / "runtime"
        runtime.mkdir(mode=0o700)
        env = dict(os.environ)
        for key in ("WAYLAND_SOCKET", "WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS", "DBUS_SYSTEM_BUS_ADDRESS", "QT_WAYLAND_SHELL_INTEGRATION", "QT_QPA_PLATFORM", "QT_QPA_PLATFORMTHEME", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH", "LD_PRELOAD", "PIPEWIRE_REMOTE", "PIPEWIRE_RUNTIME_DIR", "WIREPLUMBER_CONFIG_DIR", "KWIN_SCREENSHOT_NO_PERMISSION_CHECKS", "KWIN_WAYLAND_NO_PERMISSION_CHECKS", "LD_LIBRARY_PATH", "MESA_LOADER_DRIVER_OVERRIDE", "GALLIUM_DRIVER"):
            env.pop(key, None)
        env.update(HOME=str(root), XDG_RUNTIME_DIR=str(runtime), XDG_CONFIG_HOME=str(root / "config"),
            XDG_DATA_HOME=str(root / "data"), XDG_CACHE_HOME=str(root / "cache"), XDG_STATE_HOME=str(root / "state"),
            DBUS_SYSTEM_BUS_ADDRESS="unix:path=" + str(root / "no-system-bus"), QT_QPA_PLATFORM="wayland",
            QT_QUICK_BACKEND="software", KWIN_COMPOSE="O2", LIBGL_ALWAYS_SOFTWARE="1", QT_STYLE_OVERRIDE="Fusion", QT_FATAL_WARNINGS="1",
            QINDAQT_CAPTURE_TEST_HELPER=str(helper), QINDAQT_PORTAL_TEST_HELPER=str(consent), QINDAQT_PORTAL_TEST_RELAY=str(relay),
            QINDAQT_CAPTURE_TEST_AUDIT=str(runtime / "qindaqt-capture.audit"), QINDAQT_CAPTURE_TEST_PIXELS=str(pixels), QINDAQT_PORTAL_FOREIGN_EXPORTER=str(exporter), QINDAQT_PORTAL_TEST_MODE="grant-choices",
            QINDAQT_PORTAL_TEST_AUDIT=str(root/"consent.audit"), XDG_DATA_DIRS=str(root / "empty-data"), XDG_CONFIG_DIRS=str(root / "empty-config"),
            PIPEWIRE_REMOTE="pipewire-capture", PIPEWIRE_RUNTIME_DIR=str(runtime))
        env["QINDAQT_CAPTURE_TEST_RENDERER"] = renderer
        if any(case in input_cases for case in group):
            env["QINDAQT_NATIVE_INPUT_PROTECTED_BROKER"] = "1"
        if renderer == "llvmpipe":
            env.update(MESA_LOADER_DRIVER_OVERRIDE="swrast", GALLIUM_DRIVER="llvmpipe")
        else:
            env["LIBGL_ALWAYS_SOFTWARE"] = "0"
        portals = root / "portals"
        portals.mkdir()
        # This private overlay selects the same final protected five-family
        # composition; it neither installs routes nor grants native admission.
        (portals / "qindaqt-access.portal").write_text("[portal]\nDBusName=org.freedesktop.impl.portal.desktop.qindaqt\nInterfaces=org.freedesktop.impl.portal.Access;\nUseIn=qindaqt;\n")
        (portals / "qindaqt-capture.portal").write_text("[portal]\nDBusName=org.freedesktop.impl.portal.desktop.qindaqt.capture\nInterfaces=org.freedesktop.impl.portal.Screenshot;org.freedesktop.impl.portal.ScreenCast;org.freedesktop.impl.portal.RemoteDesktop;org.freedesktop.impl.portal.InputCapture;org.freedesktop.impl.portal.Clipboard;\nUseIn=qindaqt;\n")
        (portals / "qindaqt-portals.conf").write_text("[preferred]\ndefault=none\norg.freedesktop.impl.portal.Access=qindaqt-access\norg.freedesktop.impl.portal.Screenshot=qindaqt-capture\norg.freedesktop.impl.portal.ScreenCast=qindaqt-capture\norg.freedesktop.impl.portal.RemoteDesktop=qindaqt-capture\norg.freedesktop.impl.portal.InputCapture=qindaqt-capture\norg.freedesktop.impl.portal.Clipboard=qindaqt-capture\n")
        (portals / "portals.conf").write_text("[preferred]\ndefault=none\n")
        env.update(XDG_CURRENT_DESKTOP="qindaqt", XDG_DESKTOP_PORTAL_DIR=str(portals))
        applications = root / "data/applications"
        applications.mkdir(parents=True)
        # These desktop entries grant no capture permission. Only compositor
        # control/consent leases may authorize the protected helper's pixels.
        for name, executable in (
            ("org.test.CaptureResident", broker), ("org.test.CaptureHelper", helper),
            ("org.test.CapturePixels", pixels),
        ):
            (applications / (name + ".desktop")).write_text(f"[Desktop Entry]\nType=Application\nName=Private capture fixture\nExec={executable}\nNoDisplay=true\n")
        (applications / "org.test.Capture.desktop").write_text("[Desktop Entry]\nType=Application\nName=Private capture caller\nExec=/usr/bin/true\nNoDisplay=true\n")
        # The qualified ordinary program supplies tool PATH only; it is never
        # executed as the compositor. Native plugins belong to this exact driver.
        env["PATH"] = str(qualified_support_program.parent) + os.pathsep + env.get("PATH", "/usr/bin:/bin")
        env["QT_PLUGIN_PATH"] = str(plugin_prefix)
        pwconfig = root / "pipewire-private.conf"
        pwconfig.write_text("""context.properties = { core.daemon = true core.name = pipewire-capture support.dbus = false }
context.spa-libs = { support.* = support/libspa-support }
context.modules = [
 { name = libpipewire-module-protocol-native }
 { name = libpipewire-module-metadata }
 { name = libpipewire-module-spa-node-factory }
 { name = libpipewire-module-client-node }
 { name = libpipewire-module-access }
 { name = libpipewire-module-link-factory }
]
context.objects = [ { factory = spa-node-factory args = { factory.name = support.node.driver node.name = Private-Driver priority.driver = 20000 } } ]
""")
        # Primary client context is needed by compositor/frontend/FD consumer;
        # the separate daemon config still creates no hardware/audio sources.
        (root / "client.conf").write_text(pathlib.Path("/usr/share/pipewire/client.conf").read_text())
        # Installed policy scripts only, no hardware/audio/camera monitor profile.
        wpdir = root / "wireplumber"
        wpdir.mkdir()
        wp = pathlib.Path("/usr/share/wireplumber/wireplumber.conf").read_text()
        wp = wp.replace("policy = {", "policy = {\n    support.logind = disabled\n    support.portal-permissionstore = disabled\n    script.client.access-portal = disabled", 1)
        (wpdir / "wireplumber.conf").write_text(wp)
        env["WIREPLUMBER_CONFIG_DIR"] = str(wpdir)
        env["PIPEWIRE_CONFIG_DIR"] = str(root)
        try:
            config = root / "bus.conf"
            config.write_text("<busconfig><type>session</type><listen>unix:path=" + str(root / "bus") + "</listen><auth>EXTERNAL</auth><policy context='default'><allow own='*'/><allow send_destination='*'/><allow receive_sender='*'/></policy></busconfig>")
            bus = subprocess.Popen(["dbus-daemon", "--nofork", "--config-file=" + str(config), "--print-address=1"], env=env, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, start_new_session=True)
            children.append(bus)
            if not select.select([bus.stdout], [], [], 5)[0]:
                raise RuntimeError("private broker unavailable")
            env["DBUS_SESSION_BUS_ADDRESS"] = bus.stdout.readline().decode().strip()
            if "explicitRememberRestoresProtectedSelection" in group:
                store = pathlib.Path("/usr/libexec/xdg-permission-store")
                if not store.is_file() or not os.access(store, os.X_OK): raise RuntimeError("actual private PermissionStore executable unavailable")
                env["QINDAQT_CAPTURE_REAL_PERMISSION_STORE"] = "1"
                children.append(subprocess.Popen([str(store)], env=env, start_new_session=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
            cache = subprocess.Popen(["kbuildsycoca6", "--noincremental"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
            children.append(cache)
            if cache.wait(timeout=30):
                raise RuntimeError("private desktop cache unavailable")
            audit_exit(cache)
            for name, command in [("pipewire", ["pipewire", "-c", str(pwconfig)]), ("wireplumber", ["wireplumber", "-p", "policy"])]:
                handle = (root / (name + ".log")).open("wb")
                logs.append(handle)
                producer = dict(env)
                producer.pop("QT_FATAL_WARNINGS", None)
                children.append(subprocess.Popen(command, env=producer, stdout=handle, stderr=subprocess.STDOUT, start_new_session=True))
                if name == "pipewire":
                    deadline = time.monotonic() + 10
                    while not (runtime / "pipewire-capture").exists() and time.monotonic() < deadline:
                        time.sleep(.05)
                    if not (runtime / "pipewire-capture").exists():
                        raise RuntimeError("private PipeWire unavailable")
            handle = (root / "compositor.log").open("wb")
            logs.append(handle)
            producer = dict(env)
            producer.pop("QT_FATAL_WARNINGS", None)
            if "explicitTwoMonitorBatchClosesEveryProducer" in group:
                producer["QINDAQT_PRIVATE_CAPTURE_TWO_OUTPUTS"] = "1"
            # AGENT-GUARD: the protected broker starts inside this compositor,
            # before the caller can update env. Export its actual exec-preserved
            # PID first so the consent test child retains the same peer check.
            native_driver = ["/bin/sh", "-c", 'QINDAQT_PORTAL_TEST_COMPOSITOR_PID=$$ exec "$@"',
                             "qindaqt-native-core", str(compositor), "serveNativeCapture", "-nocrashhandler"]
            comp = subprocess.Popen(native_driver, env=producer, stdin=subprocess.PIPE, stdout=handle, stderr=subprocess.STDOUT, start_new_session=True)
            children.append(comp)
            env["QINDAQT_PORTAL_TEST_COMPOSITOR_PID"] = str(comp.pid)
            env["WAYLAND_DISPLAY"] = "qindaqt-8"
            deadline = time.monotonic() + 15
            while comp.poll() is None and time.monotonic() < deadline:
                handle.flush()
                if b"CAPTURE_AUTHORITY_READY qindaqt-8 1100x820\n" in (root / "compositor.log").read_bytes():
                    break
                time.sleep(.05)
            if not (runtime / "qindaqt-8").exists() or b"CAPTURE_AUTHORITY_READY qindaqt-8 1100x820\n" not in (root / "compositor.log").read_bytes():
                raise RuntimeError("protected private EGL compositor/broker Ready unavailable")
            driver = subprocess.Popen([str(fixture), *group, "-nocrashhandler"], env=env, start_new_session=True)
            children.append(driver)
            driver_code = driver.wait(timeout=180)
            audit_exit(driver)
            if driver_code:
                sys.exit(driver_code)
            if "compositorLossWithdrawsStreamsFilesAndPendingPublication" in group:
                comp.wait(timeout=5)
                assert all(child.poll() is None for child in children if child not in (comp, cache, driver)), "other private dependency exited"
            else:
                assert all(child.poll() is None for child in children if child not in (cache, driver)), "private dependency exited"
        except BaseException:
            for handle in logs:
                handle.flush()
            for log in root.glob("*.log"):
                print(log.name, log.read_text(errors="replace")[-32768:], file=sys.stderr)
            raise
        finally:
            cleanup_errors = []
            for child in reversed(children):
                if child.stdin is not None and not child.stdin.closed:
                    child.stdin.close()
                    try:
                        child.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        pass
                if group_alive(child):
                    os.killpg(child.pid, signal.SIGTERM)
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    if group_alive(child):
                        os.killpg(child.pid, signal.SIGKILL)
                    child.wait(timeout=5)
                if group_alive(child):
                    os.killpg(child.pid, signal.SIGKILL)
                try:
                    audit_exit(child)
                except RuntimeError as error:
                    cleanup_errors.append(str(error))
            for handle in logs:
                handle.close()
            # Retain task-owned raw authority evidence before TemporaryDirectory
            # cleanup, including failures. These traces contain synthetic fixture
            # state only; this path is ignored build output, never an installation.
            evidence = fixture.parent / "native-capture-evidence" / root.name
            evidence.mkdir(parents=True, exist_ok=True)
            for log in root.glob("*.log"):
                shutil.copyfile(log, evidence / log.name)
            # The current audit uses the explicit helper path, while completed
            # requests are archived under runtime by the caller's reset().
            for snapshot in runtime.glob("qindaqt-capture-observed.png"):
                shutil.copyfile(snapshot, evidence / snapshot.name)
            for audit in (pathlib.Path(env["QINDAQT_CAPTURE_TEST_AUDIT"]), pathlib.Path(env["QINDAQT_PORTAL_TEST_AUDIT"]),
                          runtime / "qindaqt-capture-history.audit", runtime / "native-renderer.audit",
                          runtime / "producer-frame.audit", runtime / "producer-stderr.audit",
                          runtime / "producer-lifecycle.audit"):
                if audit.is_file():
                    shutil.copyfile(audit, evidence / audit.name)
            cores = [str(path.relative_to(root)) for path in root.rglob("*")
                     if path.is_file() and (path.name == "core" or path.name.startswith("core."))]
            # Unexpected task-local cores are preserved opaque, never printed.
            for name in cores:
                path = root / name
                if not path.is_symlink():
                    with path.open("rb") as core:
                        digest = hashlib.file_digest(core, "sha256").hexdigest()
                    shutil.copyfile(path, evidence / ("core-" + digest))
            report = dict(cases=group, evidence=str(evidence), cleanup_errors=cleanup_errors,
                          scoped_core_files=cores, plugin_prefix=str(plugin_prefix), renderer=renderer)
            (evidence / "audit.json").write_text(json.dumps(report, indent=2) + "\n")
            print("CAPTURE_GROUP_AUDIT " + json.dumps(report), flush=True)
            if cleanup_errors:
                raise RuntimeError("; ".join(cleanup_errors))
