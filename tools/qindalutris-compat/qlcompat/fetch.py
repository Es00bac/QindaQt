# SPDX-License-Identifier: GPL-3.0-or-later
"""Cached, polite HTTPS fetching for the compat-db generator.

Every response is written to the cache directory together with a small
metadata file recording its URL, HTTP status and retrieval time. Reruns and
--offline runs read the cache, so a generation is reproducible from the same
cache contents. Only https:// URLs are fetched.
"""

from __future__ import annotations

import datetime
import hashlib
import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

USER_AGENT = "qindalutris-compat-generator/1 (+QindaQt; ADR-0275)"
MAX_RESPONSE_BYTES = 16 * 1024 * 1024
TIMEOUT_SECONDS = 30


def utc_stamp(moment: datetime.datetime | None = None) -> str:
    moment = moment or datetime.datetime.now(datetime.timezone.utc)
    return moment.astimezone(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


class CachedResponse:
    """A cached response: status 200 with a body, or a remembered 404."""

    def __init__(self, url: str, status: int, retrieved: str, body: bytes | None):
        self.url = url
        self.status = status
        self.retrieved = retrieved
        self.body = body

    @property
    def ok(self) -> bool:
        return self.status == 200 and self.body is not None


class Fetcher:
    """Fetches through a cache directory.

    offline: never touch the network; missing cache entries return None.
    refresh: refetch even when a cached copy exists (for a fresh snapshot).
    min_interval: seconds between two network requests to the same host.
    """

    def __init__(self, cache_dir: Path, offline: bool = False, refresh: bool = False,
                 min_interval: float = 1.0, opener=None, log=None):
        self.cache_dir = Path(cache_dir)
        self.offline = offline
        self.refresh = refresh
        self.min_interval = min_interval
        self.network_requests = 0
        self._last_request: dict[str, float] = {}
        self._opener = opener or urllib.request.urlopen
        self._log = log or (lambda message: print(message, file=sys.stderr))

    def _paths(self, name: str) -> tuple[Path, Path]:
        safe = "".join(ch if ch.isalnum() or ch in "._-" else "_" for ch in name)
        if len(safe) > 120:
            safe = safe[:80] + "-" + hashlib.sha256(name.encode()).hexdigest()[:16]
        return self.cache_dir / safe, self.cache_dir / (safe + ".meta.json")

    def cached(self, name: str) -> CachedResponse | None:
        body_path, meta_path = self._paths(name)
        try:
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
            status = int(meta["status"])
            body = body_path.read_bytes() if status == 200 else None
            return CachedResponse(str(meta["url"]), status, str(meta["retrieved"]), body)
        except (OSError, ValueError, KeyError, TypeError):
            return None

    def store(self, name: str, response: CachedResponse) -> None:
        body_path, meta_path = self._paths(name)
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        if response.body is not None:
            body_path.write_bytes(response.body)
        meta = {"url": response.url, "status": response.status,
                "retrieved": response.retrieved}
        meta_path.write_text(json.dumps(meta, sort_keys=True) + "\n", encoding="utf-8")

    def _wait_for_host(self, url: str) -> None:
        host = url.split("/")[2]
        last = self._last_request.get(host)
        if last is not None:
            delay = self.min_interval - (time.monotonic() - last)
            if delay > 0:
                time.sleep(delay)
        self._last_request[host] = time.monotonic()

    def get(self, url: str, name: str) -> CachedResponse | None:
        """The response for url, from cache or network; None if unavailable."""
        if not url.startswith("https://"):
            raise ValueError(f"refusing non-https URL {url}")
        cached = self.cached(name)
        if self.offline or (cached is not None and not self.refresh):
            return cached
        self._wait_for_host(url)
        self.network_requests += 1
        request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT,
                                                       "Accept": "*/*"})
        try:
            with self._opener(request, timeout=TIMEOUT_SECONDS) as reply:
                body = reply.read(MAX_RESPONSE_BYTES + 1)
            if len(body) > MAX_RESPONSE_BYTES:
                raise ValueError("response too large")
            response = CachedResponse(url, 200, utc_stamp(), body)
        except urllib.error.HTTPError as error:
            if error.code != 404:
                self._log(f"fetch {url}: HTTP {error.code}; using cache if any")
                return cached
            response = CachedResponse(url, 404, utc_stamp(), None)
        except (urllib.error.URLError, OSError, ValueError) as error:
            self._log(f"fetch {url}: {error}; using cache if any")
            return cached
        self.store(name, response)
        return response
