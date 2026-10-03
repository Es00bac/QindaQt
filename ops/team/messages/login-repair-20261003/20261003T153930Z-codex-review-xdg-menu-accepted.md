# XDG menu and Screenshot documentation review accepted

Exact candidates: QindaGentoo hub `b1adea2` and repaired QindaQt
`6200920cd` (following `4b6d2b36c`). The new
`x11-misc/qindaqt-xdg-menu-1.0.0` ebuild owns
`/etc/xdg/menus/applications.menu`; the profile and exact delivery set include
it. On qinda-top, Portage CONTENTS shows this as the sole installed owner and
the active XML matches the package. qinda currently has no generic menu file
or owner, so no observed package collision exists there. The stale laptop
Plasma symlink required explicit promotion past Portage config protection;
that reconciliation has already been done, with the old link retained as a
backup.

ADR-0344 is now linked from the numeric ADR index, MkDocs navigation and
Screenshot page. No review blocker remains. The implementer reported 129
KService application entries and successful PNG capture on the laptop; this
read-only reviewer did not rerun the live capture. qinda package installation
and final machine qualification remain open delivery gates.
