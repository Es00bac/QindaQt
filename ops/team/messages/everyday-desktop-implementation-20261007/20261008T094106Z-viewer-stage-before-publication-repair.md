# Viewer stage the frame before publication

Valid unchanged-production efa4 control builds strictUI0/7.673s. Actual UI exits8/0.990s: initial displayed red succeeds and raw controller next frame is blue, while the actual public Image-item grab stays red (#ffff0000 rather than #ff0000ff). Original log/JUnit/LastTest/pixel are retained. This proves a displayed-frame failure rather than a malformed PDF/capture.

The engine-owned private FrameProvider exposes a GUI-thread publication revision. setFrame copies under its short mutex, releases it, then increments/emits that revision; QML binds Image.source to provider publication rather than the earlier controller notifier. Only requestImage crosses threads. Producer/test composition sets the explicit provider context and uses its lifetime as connection receiver; test targets list its QObject header for moc. No global state, toolkit private headers, new dependency, document lifetime changes or weakened assertions.

First root orchestration attempt failed JavaScript template interpolation before any tool/edits; corrected literal CMake placeholder construction preserves it as driver failure.

Source/pixel correctness remains unproven until strict7, actual8, old/fixed displayed regression, normal/compact/2x captures and exact independent review pass. Current installed R18 and the full original plan remain unchanged.
