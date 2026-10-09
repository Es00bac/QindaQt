# USB sidebar and Eject claim and cause

- Time: 2026-10-09T00:37:46+00:00
- Exact base: c265e4e661025bd400f7d0ed8c075d420b8064af
- Outcome: mounted removable drives appear automatically in File Manager and Eject uses the public Removable Media authority with visible busy/failure outcomes.
- Branch/worktree: worker/everyday-usb-fm-sol-max-20261009 / ~/work_SPaC3/container-wm.worktrees/everyday-usb-fm-sol-max-20261009
- Ownership: owner main/exporter composition; public client; File Manager media composition/presenter/QML; focused tests/docs. Manager extended ownership to the overlay removable-media atom r3/Manifest plus owning README paragraph; Windows launch paths remain prohibited.
- Acceptance: focused tests <=30 seconds, touched-target build <=120 seconds, one contained nonphysical smoke, then one component package build after source/recipe review. No physical mutation or session restart.
- Evidence: source main already publishes Devices in watch mode. Installed helper r2 SHA256 5c65b24d404d7496db1189e6ee4b2550e6d7d7cc0060bd31483867785006bb70 has org.qindaqt.RemovableMedia1/Activate strings but no Devices path/interface or MediaExporter. Manager directly observed missing live /Devices. Separate stale component must be upgraded; login alone with r2 will not repair this.
