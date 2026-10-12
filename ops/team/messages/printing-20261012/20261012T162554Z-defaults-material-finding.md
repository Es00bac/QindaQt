# Printing defaults package boundary

- Time: 2026-10-12T16:25:54+00:00
- CUPS-browsed 2.1.1 uses DNS-SD discovery by default. No legacy UDP discovery configuration is added.
- ipp-usb is absent from all configured repos; source tag 0.9.34 vendors its sole BSD-2 Go dependency. New recipe installs upstream udev activation and loopback-only config.
- qindaqt-printing first install presets only its target. Earlier administrator presets and masks win; no preset/restart on upgrades or live activation in an image root.
- Manager found an existing CUPS process needs one safe restart after jobs finish to load newly installed MIME filters. Package will tell the operator, preserving active jobs rather than guessing through a check-then-restart race.
- Focused policy tests: 10/10 passed, including real systemctl operations in disposable roots. Ebuild Bash syntax and git whitespace checks pass.
