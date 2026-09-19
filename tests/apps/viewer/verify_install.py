#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the compiled app and a relocated Viewer component without host state."""
import argparse
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zlib


def run(command, env, expected=0):
    result = subprocess.run(command, env=env, text=True, capture_output=True, timeout=35)
    assert result.returncode == expected, (command, result.returncode, result.stdout, result.stderr)
    for problem in ("ReferenceError", "TypeError", "failed to load component", "module QindaQt.Viewer contains no type"):
        assert problem not in result.stderr, result.stderr
    return result.stdout


def write_pdf(path):
    objects = [b"<< /Type /Catalog /Pages 2 0 R >>",
               b"<< /Type /Pages /Count 2 /Kids [3 0 R 5 0 R] >>",
               b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 260] /Contents 4 0 R /Resources << >> >>",
               b"<< /Length 26 >>\nstream\n1 0 0 rg 0 0 400 260 re f\n\nendstream",
               b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 260] /Contents 6 0 R /Resources << >> >>",
               b"<< /Length 26 >>\nstream\n0 0 1 rg 0 0 400 260 re f\n\nendstream"]
    data = bytearray(b"%PDF-1.4\n")
    offsets = []
    for number, value in enumerate(objects, 1):
        offsets.append(len(data))
        data.extend(f"{number} 0 obj\n".encode() + value + b"\nendobj\n")
    start = len(data)
    data.extend(b"xref\n0 7\n0000000000 65535 f \n")
    for offset in offsets:
        data.extend(f"{offset:010d} 00000 n \n".encode())
    data.extend(f"trailer\n<< /Size 7 /Root 1 0 R >>\nstartxref\n{start}\n%%EOF\n".encode())
    path.write_bytes(data)


def write_png(path):
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))
    path.write_bytes(b"\x89PNG\r\n\x1a\n"
                     + chunk(b"IHDR", struct.pack(">2I5B", 2, 1, 8, 2, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(b"\x00\xff\x00\x00\x00\xff\x00"))
                     + chunk(b"IEND", b""))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    args.root.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    for name in ("LD_LIBRARY_PATH", "LD_PRELOAD", "QML_IMPORT_PATH", "QML2_IMPORT_PATH"):
        env.pop(name, None)
    pdf = args.root / "two pages ü.pdf"
    png = args.root / "picture with spaces.png"
    write_pdf(pdf)
    write_png(png)
    assert "GPL-3.0-or-later" in run([str(args.binary), "--help"], env)
    run([str(args.binary), str(pdf), str(png)], env, expected=2)
    for name, argument, pages in (("pdf", str(pdf), 2), ("file-url", png.as_uri(), 1)):
        output = args.root / f"build-{name}.png"
        result = run([str(args.binary), "--screenshot", str(output), argument], env)
        assert f"pages={pages}" in result and "rendered=1" in result, result
        assert output.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
    run([str(args.binary), "--screenshot", str(args.root / "remote-error.png"),
         "https://example.invalid/file.pdf"], env, expected=1)
    stage = args.root / "stage"
    run(["cmake", "--install", str(args.build), "--prefix", str(stage), "--component", "Viewer"], env)
    installed = next(stage.glob("**/bin/qindaqt-viewer"))
    desktop = next(stage.glob("**/applications/org.qindaqt.Viewer.desktop"))
    contents = desktop.read_text()
    assert "Exec=qindaqt-viewer %f\n" in contents
    assert "application/pdf;image/jpeg;image/png;image/gif;image/webp;image/bmp;image/tiff;image/svg+xml;" in contents
    if shutil.which("desktop-file-validate"):
        run(["desktop-file-validate", str(desktop)], env)
    dependencies = run(["ldd", str(installed)], env)
    for library in ("libqindaqt_app_shell.so", "libqindaqt_controls_qml.so", "libqindaqt_tokens_qml.so"):
        line = next(line for line in dependencies.splitlines() if library in line)
        assert str(stage) in line, line
    assert "not found" not in dependencies, dependencies
    result = run([str(installed), "--screenshot", str(args.root / "installed-pdf.png"), str(pdf)], env)
    assert "pages=2" in result and "rendered=1" in result, result
    print("PASS: help, multi-file refusal, PDF, file URL/image, remote error, desktop metadata, staged dependencies and QindaTK UI")


if __name__ == "__main__":
    main()
