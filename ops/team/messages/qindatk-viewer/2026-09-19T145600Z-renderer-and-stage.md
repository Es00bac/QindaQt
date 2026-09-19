# Renderer and stage boundary

The implementation uses one serialized worker thread for Poppler/Qt decoding,
revision-fences obsolete results, and bounds output to 16 Mi pixels / 8192 per
edge. Password entry is masked and retained only within the current document
session. QindaTK owns all chrome and the zoom/scroll viewport. Public AppShell
owns action/menu projection and fail-closed standard menu export.

Configure succeeded; focused build is active. Tests generate genuine two-page
PDFs and images, include a synthetic encrypted PDF, and exercise GUI navigation
and 960×680 / 640×480 layouts. The manager expanded this worker's ownership to
exactly `COMPONENT Viewer` backing-library installs in AppShell, Controls and
Tokens; no other shared changes are claimed. ADR reference is now 0217.
