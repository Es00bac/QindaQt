# Viewport verification finding

2026-09-08T12:16:27-06:00: Existing views lacked scrollbars and bounded zoom input. Added three presentation-only helpers, shared selection untouched. Fatal-warning offscreen input tests using real Controls/Tokens pass 8/8 (wheel, Ctrl-wheel substeps, drag, keyboard selection and resize). Native qmltestrunner cannot initialize read-only Tokens; custom QuickTest setup composes fixture theme. Parent owns test registration and documentation.
