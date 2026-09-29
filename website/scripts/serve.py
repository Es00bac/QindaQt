#!/usr/bin/env python3
"""Serve the local showcase with byte ranges for seeking in long films."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


SITE = Path(__file__).resolve().parents[1]


class ShowcaseHandler(SimpleHTTPRequestHandler):
    def send_head(self):
        self.byte_range: tuple[int, int] | None = None
        path = Path(self.translate_path(self.path))
        requested = self.headers.get("Range", "")
        if not path.is_file() or not requested.startswith("bytes="):
            return super().send_head()
        try:
            start_text, end_text = requested[6:].split("-", 1)
            size = path.stat().st_size
            start = int(start_text) if start_text else max(0, size - int(end_text))
            end = min(size - 1, int(end_text)) if end_text and start_text else size - 1
        except (ValueError, OverflowError):
            self.send_error(400, "Invalid byte range")
            return None
        if start < 0 or start >= size or end < start:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{size}")
            self.end_headers()
            return None
        source = path.open("rb")
        self.byte_range = start, end
        self.send_response(206)
        self.send_header("Content-Type", self.guess_type(str(path)))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.send_header("Content-Length", str(end - start + 1))
        self.send_header("Last-Modified", self.date_time_string(path.stat().st_mtime))
        self.end_headers()
        return source

    def copyfile(self, source, outputfile):
        if self.byte_range is None:
            return super().copyfile(source, outputfile)
        start, end = self.byte_range
        source.seek(start)
        remaining = end - start + 1
        while remaining:
            data = source.read(min(256 * 1024, remaining))
            if not data:
                break
            outputfile.write(data)
            remaining -= len(data)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    handler = partial(ShowcaseHandler, directory=str(SITE))
    with ThreadingHTTPServer((args.host, args.port), handler) as server:
        print(f"Serving {SITE} on http://{args.host}:{args.port}/", flush=True)
        server.serve_forever()


if __name__ == "__main__":
    main()
