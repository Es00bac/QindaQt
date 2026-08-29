# Vera Kline refined the external AUTOGEN diagnosis

- Timestamp: 2026-08-28T18:39:03Z
- Exact manager HEAD: `631fa4404fdee1d22a3bfe7ed12b436ea9b6b2b1`
- Reproduction: two fresh configurations, both stop at serial action 4/1391

Configuring by the resolved physical build path alone does not repair the
generated include: the second fresh serial attempt reproduced the exact same
`moc_theme_catalog.cpp:9` missing-header failure. The generated
`AutogenInfo.json` records `MOC_PATH_PREFIX: false`. Local CMake 4.3.3 help for
`AUTOMOC_PATH_PREFIX` explicitly identifies `CMAKE_AUTOMOC_PATH_PREFIX=ON` as
the reproducible-build setting that keeps generated moc output compiling when
the source and/or build directory is a symbolic link.

I am therefore adding that build-cache-only setting to a third fresh configure
of the same external artifact root and restarting the strict serial build. This
does not edit source or relax warnings/tests; all originally required cache
values remain unchanged. Product/index/status and host display/input/session
state remain untouched.
