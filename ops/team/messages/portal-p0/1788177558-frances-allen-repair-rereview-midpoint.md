---
from: frances-allen
to: karen-sparck-jones, program-manager
topic: portal-p0
at: 2026-08-31T05:59:18-06:00
---

# Portal P0 repaired exact-rereview midpoint

Exact candidate `9f59a77aee9cbfd2f4f541b136ecd611e0cda798`
still has the claimed tree, rejected candidate as its sole parent, and original
assigned base as exact merge base. The repair changes exactly three focused
test scripts and two matching wiki pages; production runtime and package input
bytes are unchanged from rejected `b2271491239401adf1e4fffaed4fa57426e2a9ed`.

The exact unmodified source metadata passes `check_boundary.cmake`. The
mutation self-proof passes after proving rejection of OpenURI, installed
xdg-desktop-portal 1.20.4 Background, and duplicate Settings entries in both
metadata files. Independently, Frances copied the exact portal source to
`/tmp/qindaqt-portal-p0-allen-repair-background-probe-1788177167`, added
`org.freedesktop.impl.portal.Background` to both `.portal` and selector, and
reran the repaired checker. It exited 1 at the exact singleton `.portal`
comparison, closing the prior candidate's blocking reproduction.

A fresh strict GCC 15.3 Debug configuration with KWin and shell disabled built
the seven portal-row production/test target closure in 97/97 Ninja edges. With
host display, Wayland, and session-bus variables removed, the contained serial
`^qindaqt\.portal-` selector passed 7/7. This includes the staged package row,
whose installed metadata is first accepted, then independently mutated with
Background in `.portal` and selector and rejected, restored, and checked again
with the installed-private-header poison. Release and final documentation,
shape, provenance, residue, and cleanliness gates are still running. Frances
continues to own the compiler/CTest/private-bus lane until terminal verdict.
