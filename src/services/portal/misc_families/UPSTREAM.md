# Adapted upstream contracts

xdg-desktop-portal-kde v6.6.6, commit 9a5cc0e8e2b7965c5dc344cbab647cf253315835.

- print.cpp/.h: LGPL-2.0-or-later; Jan Grulich / Red Hat (2016–2017), John Layt (2007, 2010), Alex Merry (2007), Harald Sitter (2022). Standard page table also derives from Qt qpagesize.cpp as documented in print_page_sizes.cpp. The Qt/CUPS conversion and argument functions are retained with safety guards; native Request ownership and helper spooling replace KDE process/dialog glue.
- account.cpp/.h: LGPL-2.0-or-later; Red Hat / Jan Grulich (2020). User information response contract retained with native UID provider and explicit selection.
- dynamiclauncher.cpp/.h: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL; Harald Sitter (2022). This adaptation selects LGPL-3.0-only for derived policy code. Preparation result/allowlist retained; icon editing optional and unsupported.
- usb.cpp/.h: same alternative license; David Redondo (2025). This adaptation selects LGPL-3.0-only for derived policy code. Device/result tuples and writable semantics retained, with bounded subset validation.

No original corpus or upstream source files are modified. All rewritten native adapter/process code states its own SPDX license.
