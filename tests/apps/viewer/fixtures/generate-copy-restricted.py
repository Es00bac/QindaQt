#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Recreate the public copy-permission fixture using Portage-owned qpdf."""
from pathlib import Path
import hashlib
import subprocess
import tempfile

def plain_pdf():
    content = b"BT /F1 12 Tf 20 60 Td (QindaQt restricted fixture) Tj ET\n"
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 100] "
        b"/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        b"<< /Length " + str(len(content)).encode() + b" >>\nstream\n" + content + b"endstream",
    ]
    data = b"%PDF-1.7\n% SPDX-License-Identifier: GPL-3.0-or-later\n"
    offsets = [0]
    for index, value in enumerate(objects, 1):
        offsets.append(len(data))
        data += str(index).encode() + b" 0 obj\n" + value + b"\nendobj\n"
    xref = len(data)
    data += b"xref\n0 6\n0000000000 65535 f \n"
    for offset in offsets[1:]:
        data += f"{offset:010d} 00000 n \n".encode()
    data += b"trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n" + str(xref).encode() + b"\n%%EOF\n"
    return data

output = Path(__file__).resolve().parent / "copy-restricted.pdf"
with tempfile.TemporaryDirectory(prefix="qindaqt-viewer-permission-") as directory:
    source = Path(directory) / "public-input.pdf"
    source.write_bytes(plain_pdf())
    # AGENT-NOTE: deterministic encryption is exclusively for this public
    # synthetic fixture. Never reuse these settings for private documents.
    subprocess.run([
        "/usr/bin/qpdf", "--static-id", "--static-aes-iv",
        "--encrypt", "", "viewer-fixture-owner", "256",
        "--print=none", "--extract=n", "--",
        str(source), str(output),
    ], check=True)
subprocess.run(["/usr/bin/qpdf", "--check", str(output)], check=True)
print("public_fixture_sha256", hashlib.sha256(output.read_bytes()).hexdigest())
