# SPDX-License-Identifier: GPL-3.0-or-later
"""Steam's own name for an appid, used only to settle upstream conflicts.

When several upstream entries claim one Steam appid, the generator asks
https://store.steampowered.com/api/appdetails?appids=<id>&filters=basic
(the Steam store's public app-details endpoint; the answer is keyed by an id
that may differ from the one asked, so `data.steam_appid` is checked) and
keeps only the entry whose title is Steam's. Cached and rate-limited like
every fetch; unavailable means "no name", which withholds every claimant.
"""

from __future__ import annotations

import json

from .fetch import Fetcher

URL_TEMPLATE = "https://store.steampowered.com/api/appdetails?appids={appid}&filters=basic"
SOURCE_URL = "https://store.steampowered.com/"
SOURCE_ID = "steam-appdetails"


class SteamNames:
    def __init__(self, fetcher: Fetcher | None):
        self.fetcher = fetcher
        self.newest: str | None = None

    def __call__(self, appid: str) -> str | None:
        if self.fetcher is None:
            return None
        response = self.fetcher.get(URL_TEMPLATE.format(appid=appid),
                                    f"steam-appdetails-{appid}.json")
        if response is None or not response.ok:
            return None
        try:
            document = json.loads(response.body.decode("utf-8"))
        except ValueError:
            return None
        for entry in document.values() if isinstance(document, dict) else ():
            data = entry.get("data") if isinstance(entry, dict) else None
            if isinstance(data, dict) and str(data.get("steam_appid")) == appid \
                    and isinstance(data.get("name"), str):
                self.newest = max(self.newest or response.retrieved, response.retrieved)
                return data["name"]
        return None
