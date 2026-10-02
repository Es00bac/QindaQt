# Native Print, Account, DynamicLauncher and USB

These independent adapters reuse [portal foundation](../architecture/portal-foundation.md) RequestRegistry, selected PortalSession and readonly AccessConsent authority. Presentation runs in `qindaqt-portal-misc`, an ordinary Qt Widgets process bound to a freshly admitted Wayland descriptor and public ForeignParent. The resident owns actor policy and delayed replies; the helper owns widgets and print conversion. See [ADR-0337](../adr/0337-native-independent-portal-families.md).

Every delayed method publishes its standard Close object before starting a helper. Close, frontend owner loss, native session loss or lock authority loss retires the request and helper; late output cannot authorize it. Only the current same-UID portal frontend may call backend methods. One helper dialog runs at a time; competing requests fail rather than queue stale consent. The bounded helper protocol carries no public bus test seam or ambient display fallback.

Account reads the native UID login name and GECOS name, with a readable `.face`/`.face.icon` avatar or standard empty `file://` URI. The dialog presents actual values and independent sharing controls. Results are taken from the trusted provider snapshot, never copied from arbitrary helper text. Tests inject synthetic account data.

USB `AcquireDevices` consumes exactly `a(sa{sv}a{sv})` and returns a selected subset as `a(sa{sv})`. Device IDs are unique and bounded; writable is an exact boolean. The dialog labels offered devices and access mode. Results preserve the offered writable option and reject foreign or duplicated IDs. The standard frontend owns opening actual devices; this backend opens none.

DynamicLauncher implements version 1, Application and Webapp preparation, editable names and exact serialized GBytesIcon preservation. Icon editing is optional and unsupported. `RequestInstallToken` authenticates the frontend and current native authority before applying the upstream software-center application allowlist. The standard frontend issues/consumes tokens, installs/uninstalls desktop entries and checks installation policy; the backend does not fabricate these operations.

Print implements both `PreparePrint` and `Print`. Qt/CUPS settings, page geometry, standard paper IDs, duplex/copies/collation and printer options are adapted from the exact upstream implementation. A preparation token is random, application/frontend bound, one-use, limited to 32 entries and five minutes. Tokens clear on authority loss; an old frontend owner's tokens cannot be used by its replacement. Without a token Print presents a fresh dialog. Invalid explicit tokens fail.

Sandbox print input is a duplicated readable regular-file descriptor, bounded to 512 MiB. The helper reads with `pread`, retaining the sandbox's shared file offset. Output to file succeeds only after checked atomic save; CUPS/lpr output uses fixed executable selection and separate argument vectors/stdin, with no shell or untrusted command path. Success requires real normal process exit zero. Preformatted PDFs do not apply page range, number-up or rotation twice. Close kills the helper and its spool child; already submitted physical jobs cannot be recalled. No tests contact real printers or devices.

Focused gates are `qindaqt.portal-misc-policy`, `qindaqt.portal-misc-requests` and `qindaqt.portal-print-conversion`; the latter uses Qt offscreen and an injected spool runner. Production helper and resident must also compile. Routes remain fallback until these methods and gates pass and the manager accepts the candidate. Runtime Qt PrintSupport and the existing CUPS `lp`/`lpr` command are needed for physical printing; no KDE framework is added.

## Provenance

The installed freedesktop backend XML is normative. Upstream reference is xdg-desktop-portal-kde v6.6.6 commit `9a5cc0e8e2b7965c5dc344cbab647cf253315835`, `{print,account,dynamiclauncher,usb}.{cpp,h}`. Print conversion retains Jan Grulich, John Layt, Alex Merry and Harald Sitter credits; Account retains Red Hat/Jan Grulich; DynamicLauncher Harald Sitter; USB David Redondo. Source notices live beside the adapted implementation.
