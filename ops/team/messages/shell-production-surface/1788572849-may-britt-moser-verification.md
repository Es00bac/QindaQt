# May-Britt Moser — I2 verification midpoint

- Time: 2026-09-04T19:47:29-06:00
- Exact base: `a5d78e804f28b5e11df8216ef8e0995f5d9d5191`
- Material finding: the iconized applet modules introduced a real installed-runtime dependency on `QindaQt.Shell.Icons`; the applet components and production-shell components now stage that exact static QML closure, and installed consumers register it explicitly.
- Evidence: the exact Debug shell/applet/icon selector passes 130/130 after the focused builds. Release and contained desktop rows remain in progress.
