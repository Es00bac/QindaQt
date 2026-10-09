# Manager review: Windows PE File Manager source route

- Time: 2026-10-09T00:58:00+00:00
- Exact source candidate: 4e62256b961e87d2f8b6e63f8e822b85f65ff6e3
- Exact author handoff: 199ce6930
- Decision: ACCEPT source-only PE/File Manager routing boundary.

Independent review checked bounded DOS/PE/COFF/optional/section parsing,
retained open descriptor plus dev/inode/size/mtime/ctime and current canonical
path, typed Windows catalog origin, current Open With handler content digest,
explicit native-default preservation, no fallback after Windows outcome
uncertainty, and separate Starting/SetupOpened/OutcomeUnknown states. Empty
Open With selections refuse before inspecting files. The public process port
now says Refused has no side effect and uncertain submission is OutcomeUnknown;
concrete adapters must hold current registration/profile authority at dispatch.

Author evidence: one strict focused build53.110s, 5/5 CTests/79 Qt cases in
0.06375s, strict MkDocs/541-document links, bounded2CPU/2GiB unit settled.
Both changed File Manager launch route translation units compiled; the tiny
public FileBoundary wrapper was source-reviewed but not linked in the full FM/
SDK consumer. No concrete visible setup/profile/process/MIME handler or
installed .exe journey exists. Before connecting a real port, keep filesystem
inspection and profile/hash work off the File Manager GUI thread; an 8KiB
read limit does not bound disconnected-device latency. A handler desktop entry
must not be installed before its executable/setup target exists.
