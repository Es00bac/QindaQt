# Password encoding repair claim

Reviewer reproduced a P2 on exact `f56b9a7014849da8403a1f7d8937de91451e0d88`:
a synthetic revision-2 PDF protected with `café` renders when Poppler receives
Latin-1 bytes, but the same QString passed through `ViewerController::unlock`
fails because the facade currently converts to UTF-8. Installed Poppler Qt6
documents Latin-1 for both password byte arrays.

Claimed same-worktree repair: use the documented encoding, retain the synthetic
fixture and add a controller regression proving both direct renderer acceptance
and GUI-facade unlock. Rerun the four focused gates and hand one exact descendant
to the same reviewer. The next remote QindaMPV companion build is deferred until
this blocker is handed back; no host install or source changes are involved.
