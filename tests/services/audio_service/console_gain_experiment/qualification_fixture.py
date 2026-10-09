# SPDX-License-Identifier: GPL-3.0-or-later
"""Tiny producer-shaped evidence for pure tests; never calls ebuild or Portage."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from cohort_qualification import COMPILE_PHASES, private_bus_values
from evidence_io import record


def encoded(value) -> bytes:
    return (json.dumps(value, sort_keys=True) + "\n").encode()


def suite_payloads(root: Path, failed: bool = False) -> dict[str, bytes]:
    results = ["OK", "FAIL", "TIMEOUT"] if failed else ["OK", "OK", "OK"]
    registry, rows = [], []
    xml = ET.Element("testsuites", tests="3", failures=str(results.count("FAIL")),
                     errors=str(results.count("TIMEOUT")))
    group = ET.SubElement(xml, "testsuite", name="pipewire")
    for number, result in enumerate(results):
        name = f"fixture-{number}"
        command = [str(root / f"work/{name}")]
        registry.append({"name": name, "suite": ["pipewire"], "cmd": command, "timeout": 30})
        rows.append({"name": "pipewire:" + name, "result": result, "command": command,
            "is_fail": result != "OK", "returncode": 0 if result == "OK" else -6,
            "duration": 300.6 if result == "TIMEOUT" else 0.01,
            "env": {**private_bus_values(root), "ENV_UNSET": "DISPLAY XAUTHORITY"}})
        case = ET.SubElement(group, "testcase", name="pipewire:" + name)
        if result != "OK": ET.SubElement(case, "failure" if result == "FAIL" else "error")
    text = "\n".join(f"test: pipewire:fixture-{number}" for number in range(3))
    text += f"\nOk: {results.count('OK')}\nFail: {results.count('FAIL')}\nTimeout: {results.count('TIMEOUT')}\n"
    return {"registry": encoded(registry), "json": b"".join(encoded(row) for row in rows),
            "text": text.encode(), "junit": ET.tostring(xml), "testLog": b"actual fixture test phase\n"}


class EvidenceFixture:
    def __init__(self, root: Path):
        self.root = root
        self.recipe = root / "pipewire.ebuild"
        self.recipe.write_text("fixture recipe\n")
        self.snapshot = root / "suite-evidence"
        self.snapshot.mkdir()
        self.mode = "unpatched-asan-control"

    def put(self, name: str, data: bytes) -> dict:
        path = self.snapshot / (name + ".evidence")
        if path.exists(): path.chmod(0o600)
        path.write_bytes(data)
        path.chmod(0o400)
        return record(path)

    def replace(self, qualification: dict, name: str, data: bytes) -> None:
        qualification["upstreamEvidence"]["raw"][name] = self.put(name, data)

    def qualification(self, status: str = "failed", mode: str = "unpatched-asan-control") -> dict:
        tested = status != "not-requested"
        self.mode = mode
        raw = {"compileLog": self.put("compileLog", b"compile completed\n")}
        compile_step = {"name": "portage-compile",
            "argv": ["/usr/bin/ebuild", str(self.recipe), *COMPILE_PHASES], "exit": 0,
            "timedOut": False, "leaderReservedThroughCleanup": True, "ownedProcessGroup": 123,
            "sha256": raw["compileLog"]["sha256"]}
        steps = [compile_step]
        counts = {"ok": 0, "fail": 0, "timeout": 0}
        if tested:
            for key, data in suite_payloads(self.root, failed=status == "failed").items():
                raw[key] = self.put(key, data)
            steps.append({"name": "portage-test", "argv": ["/usr/bin/ebuild", str(self.recipe), "test"],
                "exit": 1 if status == "failed" else 0, "timedOut": False,
                "leaderReservedThroughCleanup": True, "ownedProcessGroup": 124,
                "sha256": raw["testLog"]["sha256"]})
            counts = {"ok": 1, "fail": 1, "timeout": 1} if status == "failed" else {"ok": 3, "fail": 0, "timeout": 0}
        raw["phases"] = self.put("phases", encoded({"mode": mode, "upstreamTestVariant": tested, "steps": steps}))
        raw["inputsFinal"] = self.put("inputsFinal", encoded({"verified": True}))
        return {"diagnosticOnly": status == "failed", "upstreamTests": status,
            "upstreamEvidence": {"schema": 1, "mode": mode, "root": str(self.root),
                "recipePath": str(self.recipe), "compileExit": 0,
                "testExit": (1 if status == "failed" else 0) if tested else None,
                "suiteComplete": tested, "testEnvironmentVerified": tested,
                "registryCount": 3 if tested else 0, "counts": counts, "raw": raw}}

    def json_of(self, qualification: dict, key: str):
        return json.loads(Path(qualification["upstreamEvidence"]["raw"][key]["path"]).read_bytes())
