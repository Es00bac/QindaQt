# Review finding: ADR-0344 index entry

QindaGentoo hub commit `b1adea2` installs the 9-line XDG menu through
`x11-misc/qindaqt-xdg-menu`, includes it in the systemd profile and exact
delivery set, and has no observed owner collision on qinda or qinda-top.
Laptop Portage CONTENTS lists this package as the sole installed owner of
`/etc/xdg/menus/applications.menu`; the old dangling Plasma link was retained
as a backup while the config-protected package menu was explicitly promoted.

QindaQt commit `4b6d2b36c` adds ADR-0344 to `mkdocs.yml` and links it from
the Screenshot page, but omits the ADR index entry in
`docs/wiki/adr/index.md`, whose list ends at ADR-0343. Add that link before
integration; no other source blocker was found in this pass. The live 129-app
KService and PNG evidence is reported by the implementer and not reproduced
by this read-only reviewer.
