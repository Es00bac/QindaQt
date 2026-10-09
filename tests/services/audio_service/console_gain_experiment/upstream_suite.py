# SPDX-License-Identifier: GPL-3.0-or-later
"""Complete Meson registry/JSON/text/JUnit observation, never executes a test."""
from __future__ import annotations
from collections import Counter
import math
import re
import xml.etree.ElementTree as ET
from evidence_io import json_value


def inspect_suite(payloads: dict[str, bytes], expected_environment: dict[str, str]) -> dict:
    registry = json_value(payloads["registry"])
    if not isinstance(registry, list) or not 0 < len(registry) <= 256:
        raise ValueError("missing or unbounded compiled test registry")
    declared = {}
    for entry in registry:
        if (not isinstance(entry, dict) or entry.get("suite") != ["pipewire"]
                or not isinstance(entry.get("name"), str) or not 0 < len(entry["name"]) <= 256
                or not isinstance(entry.get("cmd"), list) or not 0 < len(entry["cmd"]) <= 64
                or any(not isinstance(arg, str) or not 0 < len(arg) <= 4096 for arg in entry["cmd"])):
            raise ValueError("unknown compiled registry entry")
        name = "pipewire:" + entry["name"]
        if name in declared:
            raise ValueError("duplicate compiled registry identity")
        declared[name] = entry["cmd"]
    lines = payloads["json"].splitlines()
    if len(lines) != len(declared):
        raise ValueError("suite is incomplete or has extra observations")
    observed = {}
    for line in lines:
        row = json_value(line)
        if (not isinstance(row, dict) or row.get("name") not in declared
                or row["name"] in observed or row.get("command") != declared[row["name"]]
                or row.get("result") not in {"OK", "FAIL", "TIMEOUT"}
                or type(row.get("returncode")) is not int
                or type(row.get("duration")) not in {int, float}
                or not math.isfinite(row["duration"]) or not 0 <= row["duration"] <= 7200
                or row.get("is_fail") is not (row.get("result") != "OK")):
            raise ValueError("unknown, repeated or contradictory actual test observation")
        if (row["returncode"] == 0) != (row["result"] == "OK"):
            raise ValueError("test result and return code contradict")
        environment = row.get("env")
        if (not isinstance(environment, dict)
                or any(environment.get(key) != value for key, value in expected_environment.items())):
            raise ValueError("actual test phase did not retain both private bus addresses")
        unset_value = environment.get("ENV_UNSET", "")
        if not isinstance(unset_value, str):
            raise ValueError("malformed actual ENV_UNSET")
        unset = unset_value.split()
        if any(key in unset for key in expected_environment):
            raise ValueError("actual ENV_UNSET contradicts private test environment")
        observed[row["name"]] = row["result"]
    counts = Counter(observed.values())
    wanted = {"ok": counts["OK"], "fail": counts["FAIL"], "timeout": counts["TIMEOUT"]}
    text = payloads["text"].decode("utf-8")
    for label, key in (("Ok", "ok"), ("Fail", "fail"), ("Timeout", "timeout")):
        values = re.findall(r"(?m)^" + label + r":\s*(\d+)\s*$", text)
        if (not values and wanted[key] != 0) or len(values) > 1:
            raise ValueError("full text suite summary missing or repeated")
        if values and int(values[0]) != wanted[key]:
            raise ValueError("text and JSON suite totals contradict")
    if text.count("test:") < len(declared):
        raise ValueError("text suite observations are incomplete")
    xml = ET.fromstring(payloads["junit"])
    if xml.tag != "testsuites" or xml.get("tests") != str(len(declared)):
        raise ValueError("JUnit registry count contradicts")
    if xml.get("failures") != str(wanted["fail"]) or xml.get("errors") != str(wanted["timeout"]):
        raise ValueError("JUnit totals contradict actual JSON")
    cases = list(xml.iter("testcase"))
    if len(cases) != len(declared):
        raise ValueError("JUnit is incomplete")
    seen = set()
    for case in cases:
        name = case.get("name")
        flags = [child.tag for child in case if child.tag in {"failure", "error", "skipped"}]
        expected = {"OK": [], "FAIL": ["failure"], "TIMEOUT": ["error"]}.get(observed.get(name))
        if name in seen or expected is None or flags != expected:
            raise ValueError("JUnit identity/status contradicts")
        seen.add(name)
    return {"registryCount": len(declared), "counts": wanted,
            "upstreamTests": "failed" if wanted["fail"] + wanted["timeout"] else "passed"}
