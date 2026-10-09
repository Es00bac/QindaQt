# ADR-0370: Admit user-selected Windows programs through leased PE files

- Status: Proposed
- Date: 2026-10-09
- Owners: Windows and File Manager delivery
- Related: [ADR-0352](0352-foreign-runtime-identity-and-installation.md), [ADR-0366](0366-coordinate-ordinary-windows-launches-through-public-ports.md)
- Scope: extend the package-only Windows launch proposal for explicitly selected local Windows programs; package-managed guest recipes remain a separate workflow

## Context

The owner explicitly requires valid local `.exe` programs to open from File Manager by double-click or Open With, with an existing chosen Wine/Proton profile or visible first-run setup. A renamed script is not a Windows executable, and an extension, desktop Exec or window title cannot establish managed provenance. The original packaged-recipe flow alone cannot handle this user-selected-file outcome.

## Decision

Keep Portage-owned runtime/host installation and packaged guest recipes unchanged. Add a separate user-selected local program workflow through the public WindowsRunner executable-open boundary. Its source provenance is a user-file receipt, never a fabricated Portage guest package. The setup owner records the exact selected file/digest, app UUID, chosen runtime/profile revision and portable/installer choice before registration. Unknown programs require visible setup; installers and portable apps are distinguished by explicit choice or an admitted recipe, not filename guessing. Prefix and data are retained per app; failure, unregister and update do not implicitly delete them.

Admit the actual opened regular file using bounded DOS/MZ, PE signature, COFF, optional-header and executable-section validation. Keep its device/inode/size/mtime/ctime and open handle through setup and dispatch, detecting replacement and content change. This verifies supported executable format; it does not establish software safety, package provenance or a compositor association. Architecture compatibility is a separate runtime/profile check.

Both File Manager routes consume the same public port. Current user defaults and the explicitly chosen scan-retained Open With handler decide whether the Windows route applies; deliberate native handlers remain intact. A selected Windows route never falls back or retries after submission. Its states distinguish Starting, real visible SetupOpened, failure and OutcomeUnknown. Any uncertain result after submission is OutcomeUnknown, while refusal/default fallback requires no operational side effect.

At actual process dispatch the adapter holds current registration/profile authority transactionally and consumes checked payload/file handles. A copied coordinator recheck is insufficient to close the final removal/update race. Shell interpolation, game library/store internals, manual host software installation and automatic guest downloads are excluded.

## Consequences and current evidence

The typed managed catalog bridge, PE/file-identity inspector and injected File Manager double-click/Open With routing are implemented source boundaries. The focused smoke uses header-format fixtures and an injected runtime/setup port; it does not run Wine or claim an installed user journey. A concrete user-file receipt/profile owner, visible setup UI, process adapter and compiled Portage-packaged desktop/MIME handler remain required. Handler data is added only with its executable; XDG user defaults must continue to override the packaged fallback.

The normal Applications launch, authenticated blue Windows member identity, Audio1 and same-process standard-HMENU Global Menu remain independent required outcomes. See [WindowsRunner](../architecture/windows-runner.md) and [File Manager](../apps/file-manager.md).
