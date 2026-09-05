# May-Britt Moser — Shell iconography I2 midpoint

- Timestamp: 2026-09-04T18:58:21-06:00
- Exact base: `a5d78e804f28b5e11df8216ef8e0995f5d9d5191`.
- Material finding: Audio's full 340-pixel details surface was mounted directly in the 28-pixel panel strip, producing the reported clipping. The panel now owns only a compact summary icon and opens those unchanged controls in a bounded popup.
- Direction: a shell-owned, constructor-visible configuration seam consumes the optional `iconTheme` hint from the selected validated theme, derives `breeze-dark`/`breeze` when absent, and installs I1 before panel QML. Applet state and mutation authority remain in existing controllers.
- Current verification: Debug configuration completed with exit 0; focused compilation is in progress. No runtime evidence is claimed yet.
