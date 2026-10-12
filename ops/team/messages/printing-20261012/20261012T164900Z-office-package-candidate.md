# Office r3 recipe review candidate

- Time: 2026-10-12T16:49:00+00:00
- Overlay candidate: 9f6eed509cfe027bced4e1b00b4ef1a15d4e35be, pushed to qinda hub on build/office-printing-r3-20261012
- Office source: d47c4d905534bea0b2d65394eebf35dc5b7bbab5
- QXlsx: unchanged 4e82d6c0726dcc39020cc5491d0de77f787c18ef
- Changed paths: gui-apps/qindaoffice/qindaoffice-0.1.0_p20261010-r3.ebuild; gui-apps/qindaoffice/Manifest; metadata/qinda-delivery Office row.
- Both independent clean tools/make-dist.sh runs (qinda/laptop): exit 0, 8240160 bytes, SHA256 23d62cc4602bd1f856a18f3db3cd5a66415dba6593bd1a509714d39725a4e97e. Archive source marker, vendor tree, 3839-entry single-prefix inventory and absence of Git administrative files pass.
- Ebuild Manifest generation, Bash syntax, whitespace and Portage metadata boundary assertions: exit 0. Qt CUPS is build+runtime; independent printing stack is runtime-only.
- Qinda installed baseline directly checked: Office r2, toolkit r18 and Qt6.11.1[cups]. MAKEOPTS remains -j24 -l24. Isolated source and overlay worktrees live under ~/work_SPaC3/.qa/office-printing-r3-20261012.
- Next: exact independent source/recipe review, then one full Portage buildpkgonly. No host merge, printer or session action authorized to this worker.
