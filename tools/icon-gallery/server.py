#!/usr/bin/env python3
"""Read-only, loopback-only gallery for a theme's current working PNG files."""
import argparse
import csv
import hashlib
import json
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("theme", type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--port", type=int, default=8768)
    parser.add_argument("--state-file", type=Path)
    args = parser.parse_args()
    theme = args.theme.resolve(strict=True)
    page = Path(__file__).with_name("index.html").read_bytes()
    expected = {}
    if args.manifest:
        with args.manifest.open() as stream:
            expected = {r["output_path"]: r["source_sha256"] for r in csv.DictReader(stream)}

    class Handler(BaseHTTPRequestHandler):
        def send(self, body, mime, cache="no-store"):
            self.send_response(200)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", cache)
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            path = unquote(urlsplit(self.path).path)
            if path == "/":
                self.send(page, "text/html; charset=utf-8")
            elif path == "/api/inventory":
                icons = []
                signature = hashlib.blake2s()
                for file in sorted(theme.rglob("*.png")):
                    try:
                        if not file.resolve().is_relative_to(theme):
                            continue
                        stat = file.stat()
                    except FileNotFoundError:
                        continue  # An in-progress replacement is visible on the next refresh.
                    relative = file.relative_to(theme).as_posix()
                    version = str(stat.st_mtime_ns)
                    icons.append({"path": relative, "category": file.parent.name,
                                  "name": file.stem, "version": version})
                    signature.update(f"{relative}:{version}:{stat.st_size}\n".encode())
                canonical = {expected[i["path"]] for i in icons if i["path"] in expected}
                body = {"icons": icons, "present": len(icons), "expected": len(expected),
                        "canonical_present": len(canonical), "version": signature.hexdigest(),
                        "checked_at": datetime.now(timezone.utc).isoformat()}
                self.send(json.dumps(body).encode(), "application/json")
            elif path.startswith("/icons/"):
                # AGENT-GUARD: this endpoint serves PNGs within the selected theme only.
                # Never turn this preview into an arbitrary local-file HTTP server.
                file = (theme / path.removeprefix("/icons/")).resolve()
                if not file.is_relative_to(theme) or file.suffix.lower() != ".png":
                    self.send_error(404)
                else:
                    try:
                        self.send(file.read_bytes(), "image/png", "private, max-age=3600")
                    except FileNotFoundError:
                        self.send_error(404)
            else:
                self.send_error(404)

        def log_message(self, *_args):
            pass

    with ThreadingHTTPServer(("127.0.0.1", args.port), Handler) as server:
        server.daemon_threads = True
        url = f"http://127.0.0.1:{server.server_port}/"
        if args.state_file:
            args.state_file.parent.mkdir(parents=True, exist_ok=True)
            args.state_file.write_text(json.dumps({"url": url, "theme": str(theme)}))
        print(url, flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
