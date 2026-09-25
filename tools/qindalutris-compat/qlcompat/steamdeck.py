# SPDX-License-Identifier: GPL-3.0-or-later
"""Valve's Steam Deck compatibility rating ("Deck Verified").

Reads https://store.steampowered.com/saleaction/ajaxgetdeckappcompatibility
report?nAppID=<appid>&l=english, the undocumented endpoint the Steam store
page uses: {success: 1, results: {resolved_category: 3|2|1|0,
resolved_items: [{display_type, loc_token}]}}; `results` is an empty list
for an appid Valve has not rated. Categories: 3 Verified, 2 Playable,
1 Unsupported, 0 Unknown. Observed display types: 4 is a passed check, 3 a
limitation, 2 a blocker, 1 advice; every item except passed checks becomes
a note, through SENTENCES where the token is known. Being undocumented, it is treated like ProtonDB: the same
appids in the same order under the same limit, cached, rate-limited, and
skipped on any error.
"""

from __future__ import annotations

import json
import re

from . import protondb
from .fetch import Fetcher
from .model import Registry, clean_text

URL_TEMPLATE = ("https://store.steampowered.com/saleaction/"
                "ajaxgetdeckappcompatibilityreport?nAppID={appid}&l=english")
SOURCE_URL = "https://store.steampowered.com/"
SOURCE_ID = "steam-deck"
CATEGORIES = {3: "verified", 2: "playable", 1: "unsupported"}
PASSED = 4


# Valve's result tokens seen in the reports, as plain sentences. The wording
# is ours, written from the token names and the Steam store's own
# explanations of each result; an unlisted token falls back to readable().
SENTENCES = {
    "AudioOutputHasNonblockingIssues": "Audio output has minor issues.",
    "ControllerGlyphsDoNotMatchDeckDevice":
        "Some on-screen button prompts show keyboard, mouse or other controllers' icons.",
    "DefaultConfigurationIsNotPerformant":
        "The default graphics settings do not run well; lower them.",
    "DefaultControllerConfigNotFullyFunctional":
        "Some functions are not reachable with the default controller layout.",
    "DeviceCompatibilityWarningsShown": "The game warns that the device may not be supported.",
    "DisplayOutputHasNonblockingIssues": "Display output has minor issues.",
    "ExternalControllersNotSupportedPrimaryPlayer":
        "An external controller is not used as the first player's controller by default.",
    "FirstTimeSetupRequiresActiveInternetConnection":
        "First-time setup needs an internet connection.",
    "GameOrLauncherDoesntExitCleanly": "The game or its launcher does not close cleanly.",
    "GamepadNotEnabledByDefault": "Controller support has to be switched on in the game's settings.",
    "InterfaceTextIsNotLegible": "Some text is small and may be hard to read.",
    "LauncherInteractionIssues":
        "The game's launcher needs the touchscreen or on-screen keyboard, or is hard to read.",
    "NativeResolutionNotDefault":
        "The game supports the screen's native resolution but does not use it by default.",
    "NativeResolutionNotSupported": "The game does not support the screen's native resolution.",
    "SingleplayerGameplayRequiresActiveInternetConnection":
        "Single-player needs an internet connection.",
    "SteamOSDoesNotSupport": "Some or all of the game does not work on SteamOS.",
    "SteamOSDoesNotSupport_Retired": "The game has been retired and does not run on SteamOS.",
    "SteamOSDoesNotSupport_VR": "The game is VR-only, which SteamOS does not support.",
    "TextInputDoesNotAutomaticallyInvokesKeyboard":
        "Typing text needs the on-screen keyboard to be opened by hand.",
    "UnsupportedAntiCheatConfiguration":
        "The game's anti-cheat is not set up to allow SteamOS.",
    "UnsupportedAntiCheat_Other": "The game's anti-cheat does not support SteamOS.",
    "UnsupportedGraphicsPerformance": "Graphics performance is not good enough.",
}


def _result_name(token: str) -> str:
    """Everything after "_TestResult_" (the whole remainder, underscores kept)."""
    token = token.lstrip("#")
    marker = "_TestResult_"
    return token[token.index(marker) + len(marker):] if marker in token else token


def readable(token: str) -> str:
    """A token as a sentence: the table, else the whole remainder split into words.

    "#SteamDeckVerified_TestResult_InterfaceTextIsNotLegible" ->
    "Some text is small and may be hard to read."
    """
    name = _result_name(token)
    if name in SENTENCES:
        return SENTENCES[name]
    words = []
    for part in name.split("_"):
        words += re.findall(r"[A-Z]+(?=[A-Z][a-z]|[0-9]|$)|[A-Z]?[a-z]+|[0-9]+|[A-Z]+", part)
    if not words:
        return ""
    text = " ".join(w if w.isupper() and len(w) > 1 else w.lower() for w in words)
    return clean_text(text[:1].upper() + text[1:] + ".", 300)


def parse(data: bytes) -> dict | None:
    """The steamDeck object for one report, or None when Valve has no rating."""
    document = json.loads(data.decode("utf-8"))
    results = document.get("results") if isinstance(document, dict) else None
    if document.get("success") != 1 or not isinstance(results, dict):
        return None
    category = CATEGORIES.get(results.get("resolved_category"))
    if category is None:
        return None
    notes = []
    for item in results.get("resolved_items") or []:
        if not isinstance(item, dict) or item.get("display_type") == PASSED:
            continue
        note = readable(str(item.get("loc_token", "")))
        if note and note not in notes:
            notes.append(note)
    return {"category": category, "notes": notes} if notes else {"category": category}


def apply(registry: Registry, fetcher: Fetcher, limit: int, log) -> str | None:
    """Set Steam Deck ratings; returns the newest retrieval stamp used."""
    newest = None
    for appid, record in protondb.candidates(registry)[:max(0, limit)]:
        response = fetcher.get(URL_TEMPLATE.format(appid=appid), f"steamdeck-{appid}.json")
        if response is None or not response.ok:
            continue
        try:
            deck = parse(response.body)
        except (ValueError, AttributeError):
            log(f"steam-deck: unreadable report for {appid}")
            continue
        if deck is None:
            continue
        record.deck = deck
        record.extend("links", [f"https://store.steampowered.com/app/{appid}/"])
        newest = max(newest or response.retrieved, response.retrieved)
    return newest
