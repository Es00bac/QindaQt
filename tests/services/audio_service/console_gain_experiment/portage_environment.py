# SPDX-License-Identifier: GPL-3.0-or-later
"""Private folder/process environment composition; no Portage import or launch."""
from __future__ import annotations
import json
import os
from pathlib import Path
from portage_policy import cohort_features
from cohort_qualification import preserved_env_unset


def prepare_environment(output: Path, group: int, portage_uid: int,
                        installed: dict[str, str], overlay: Path, gentoo: Path, distdir: Path,
                        features: set[str], env_unset: str, *,
                        asan: bool, upstream_tests: bool) -> tuple[dict[str, str], bool]:
    # AGENT-GUARD: literal private addresses must survive setup/saved EAPI env.
    # No host loader/DBus/XDG values enter this explicit child composition.
    directories = {}
    for key, folder in {"PORTAGE_TMPDIR": "tmp", "PKGDIR": "packages",
                        "PORTAGE_LOGDIR": "logs", "CCACHE_DIR": "ccache"}.items():
        directory = output / folder
        directory.mkdir(mode=0o750); os.chown(directory, -1, group)
        directories[key] = str(directory)
    environment = {"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8", "LC_ALL": "C.UTF-8",
        "PORTAGE_REPOSITORIES": ("[DEFAULT]\nmain-repo = gentoo\n"
            + "[gentoo]\nlocation = " + str(gentoo) + "\nauto-sync = no\n"
            + "[qindaqt]\nlocation = " + str(overlay) + "\nauto-sync = no\n"),
        "DISTDIR": str(distdir), "MAKEOPTS": "-j8 -l8",
        "BINPKG_FORMAT": "gpkg", **directories,
        "DBUS_SESSION_BUS_ADDRESS": f"unix:path={output}/absent-session",
        "DBUS_SYSTEM_BUS_ADDRESS": f"unix:path={output}/absent-system"}
    for key in ("CFLAGS", "CXXFLAGS", "LDFLAGS", "CHOST", "CBUILD"):
        environment[key] = installed[key]
    test_variant = asan or upstream_tests
    environment["USE"] = "-* " + installed["USE"] + (" test" if test_variant else "")
    # Flags/USE come from the exact installed cohort; signing/features are the
    # current manager-owned Portage policy, which may have advanced since that
    # installed package was built. No key or configuration is rewritten.
    environment["ENV_UNSET"] = preserved_env_unset(env_unset)
    (output / "portage-feature-baseline.json").write_text(
        json.dumps(sorted(features)) + "\n")
    features = cohort_features(features, asan=asan, upstream_tests=upstream_tests)
    if asan:
        flags = " -fsanitize=address -fno-omit-frame-pointer -g3 -O1"
        environment["CFLAGS"] += flags
        environment["CXXFLAGS"] += flags
        environment["LDFLAGS"] += " -fsanitize=address"
    environment["FEATURES"] = "-* " + " ".join(sorted(features))
    for key, folder in {"XDG_RUNTIME_DIR": "runtime", "XDG_CONFIG_HOME": "config",
                        "XDG_STATE_HOME": "state", "XDG_CACHE_HOME": "cache",
                        "XDG_DATA_HOME": "data"}.items():
        directory = output / folder
        directory.mkdir(mode=0o700)
        os.chown(directory, portage_uid, group)
        environment[key] = str(directory)
    environment.update(PIPEWIRE_RUNTIME_DIR=environment["XDG_RUNTIME_DIR"],
                       PIPEWIRE_REMOTE="pipewire-0", PIPEWIRE_CORE="pipewire-0",
                       XDG_CONFIG_DIRS=environment["XDG_CONFIG_HOME"],
                       XDG_DATA_DIRS=environment["XDG_DATA_HOME"])
    return environment, test_variant


def admit_effective_environment(environment: dict[str, str], settings_factory) -> dict:
    """Read-only config composition before dispatch; borrowed factory, no phases.

    The supplied ENV_UNSET contains incremental override tokens. The observed
    expanded value must not contain either name; raw caller intent is not proof.
    Saved phase and per-test exact bus checks remain independently mandatory.
    """
    settings=settings_factory(env=environment)
    effective=settings.get("ENV_UNSET","")
    if not isinstance(effective,str):
        raise ValueError("invalid effective ENV_UNSET")
    bus_names=("DBUS_SESSION_BUS_ADDRESS","DBUS_SYSTEM_BUS_ADDRESS")
    if any(name in effective.split() for name in bus_names):
        raise ValueError("effective ENV_UNSET filters private bus")
    if any(settings.get(name)!=environment.get(name) for name in bus_names):
        raise ValueError("effective private bus address changed")
    return {"ENV_UNSET":effective,
            **{name:settings.get(name) for name in bus_names}}


def admit_saved_environment(path: Path, expected: dict[str, str]) -> dict:
    """Parse only three exported plain values; never source saved shell code."""
    import re,shlex,stat
    descriptor=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        info=os.fstat(descriptor)
        if not stat.S_ISREG(info.st_mode) or info.st_size>16777216:
            raise ValueError("saved environment file refused")
        with os.fdopen(os.dup(descriptor),"r") as stream:
            text=stream.read(16777217)
        if len(text)>16777216:raise ValueError("saved environment bound")
    finally:os.close(descriptor)
    values={}
    for line in text.splitlines():
        match=re.fullmatch(r"declare -x (ENV_UNSET|DBUS_SESSION_BUS_ADDRESS|DBUS_SYSTEM_BUS_ADDRESS)=(.*)",line)
        if not match:continue
        key,raw=match.groups()
        if key in values:raise ValueError("duplicate saved private environment")
        tokens=shlex.split(raw)
        if len(tokens)!=1:raise ValueError("malformed saved private environment")
        values[key]=tokens[0]
    if set(values)!={"ENV_UNSET","DBUS_SESSION_BUS_ADDRESS","DBUS_SYSTEM_BUS_ADDRESS"}:
        raise ValueError("saved private environment missing")
    return admit_effective_environment(expected,lambda **unused:values)
