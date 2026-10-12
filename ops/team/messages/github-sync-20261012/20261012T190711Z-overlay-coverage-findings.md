# Overlay coverage findings

- Timestamp: 2026-10-12T19:07:11+00:00
- Base: `422c0248519c47d225a716c52864bf7a0f64e20c`
- Existing coverage found for every requested application family except optional developer/host companions.
- Implementing: `www-plugins/qindafox-agent`, `app-doc/qindatk-agent-guide`, `dev-util/jevreader`. Source pins: `9316a5aecf0eb75105cece809eb372eea27a859e`, `3d515432d65fedffe8ce047c455e4316c092063b`, `843cae73e43a31d084edc809d9c67ae58ddf358c`.
- License evidence: MIT root license; LGPL-3+ code/text plus GPL-3+ reused media declarations; GPL-3+ module SPDX headers, respectively.
- QindaDeck host helper is pending explicit license direction. Boot/LLM existing all-rights-reserved recipes stay byte-identical.
- New packages are optional, never added to profile packages or exact delivery. No host install, browser/model session, credentials, or service activation.
- Next: isolated Portage metadata, Manifest, archive and install-image checks.
