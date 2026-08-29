# Cora Vale checkpoint: Controls non-host qualification is complete

- **Timestamp:** 2026-08-28T03:26:03Z
- **Status:** all Controls-owned gates clean; final diff audit in progress

The independent Release configuration and full serial build completed at exit
0, including all 1,307 Ninja steps. Release discovery is exactly 29 Controls
tests and the complete selector passed **29/29** at exit 0 in 16.52 seconds.
The corresponding Debug selector passed **29/29** at exit 0 in 14.99 seconds.

Detailed Release package/memory evidence passed 2/2 at exit 0:

- installed `QindaQt.Controls 1.0` consumer: passed from a clean staged prefix;
- median bare Qt Quick PSS: 17,023 KiB;
- median token-plus-Controls PSS: 37,322 KiB;
- median measured delta: 20,299 KiB;
- threshold: deliberately null.

Final static/documentation evidence:

- `qindaqt_controls_qml_qmllint`: exit 0;
- `all_qmllint`: exit 0 (existing shell-preview warnings only, none in
  Controls);
- `./tools/validate-docs`: 46 Markdown documents and nav, exit 0;
- strict MkDocs offline build: exit 0;
- source-shape: 818 files, exit 0; largest changed source is the 496-line
  behavior test and production files remain below policy limits;
- `git diff --check`: clean;
- exactly 14 public Controls QML files and 25 reviewed baseline PNGs.

The reproducible unrelated `shell.production-surface.1080p` broad timeout is
the only bounded caveat; no broad registry pass is claimed and no shell source
was edited. I am auditing the exact staged diff, then will create one immutable
milestone commit and request different-worker review.
