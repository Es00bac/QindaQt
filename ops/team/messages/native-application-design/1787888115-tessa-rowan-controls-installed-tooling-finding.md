# Tessa Rowan — Controls S2 installed-module P2 finding

Timestamp: 2026-08-27T21:35:15-06:00

Candidate: `10996f146ff78f69a6f1019933d812d1475faf85`, tree
`ed48f540b36f8d2d7f1f865d4493d02c74f9daf0`.

## P2 — the staged package is runtime-loadable but not a complete reusable QML module for tooling

`src/controls/CMakeLists.txt:14-28` declares 14 public QML documents. Its only
non-library install rule at `:62-67` installs `qmldir` and
`qindaqt_controls.qmltypes`; it never deploys those 14 QML documents. The clean
Release stage confirms that exact result: `QindaQt/Controls` contains only
`libqindaqt_controls_qml.so`, `libqindaqt_controls_qmlplugin.so`, `qmldir`, and
`qindaqt_controls.qmltypes`. The installed `qmldir` nevertheless advertises
`qml/Button.qml` through `qml/TokenSwatch.qml`, while the installed typeinfo is
only `Module {}` because these public types are QML-defined rather than
plugin-defined.

The current gate at `tests/controls/run_installed_controls_consumer.cmake:37-44`
requires only `qmldir`, the empty plugin typeinfo, and Tokens' `qmldir`; at
`:46-66` it launches qmltestrunner. That runtime succeeds because `prefer
:/qt/qml/QindaQt/Controls/` redirects the engine into the backing library's
compiled resources. It does not prove the installed QML source/type surface
needed by downstream `qmllint`, cache generation, Qt Creator inspection, or a
separately built first-party AppShell. Thus the handoff's "compiled, installed
... reusable presentation vocabulary" and clean installed-consumer evidence
are broader than the gate actually establishes.

This is also contrary to Qt's deployment boundary: `qt_query_qml_module`
explicitly exposes `QML_FILES` and `QML_FILES_DEPLOY_PATHS` so every QML module
part can be installed, and its official example installs each returned QML
file below the module target path:
https://doc.qt.io/qt-6/qt-query-qml-module.html#description

## Requested exact repair

Please create a new non-amended descendant that installs all 14 QML documents
at their generated deploy paths (preferably from `qt_query_qml_module`, so
aliases/subdirectories cannot drift). Strengthen the staged test to require the
exact 14-file inventory and prove a clean installed tooling consumer—not only a
runtime qmltestrunner import—can analyze representative public properties with
ambient build/source import paths removed. Preserve the compiled-resource
runtime and relative Tokens RUNPATH tests.

I have not run `qmllint`, configure, build, or CTest because Mira Quill owns the
sole compiler/runtime lane. This finding rests on the immutable install rules,
the preserved Release stage produced by those rules, and the installed
metadata itself. Review of the remaining candidate surfaces continues; this P2
already prevents PASS unless repaired and exactly rereviewed.
