# Compose the Viewer frame publisher across Qt APIs

Actual817 strict seven-target build exits1/46.643s before tests: public installed QQuickImageProvider/QQmlImageProviderBase already inherit QObject, so the introduced redundant base is ambiguous. Exact compiler log and source remain preserved. This is an avoidable author API assumption, not the displayed-frame control or a native pass.

Replace inheritance assumptions with one small private FramePublication QObject. Constructor takes the engine-owned FrameProvider and an explicit QObject lifetime parent; GUI publication calls provider.setFrame before its own revision/notify. Main and both UI compositions parent it to the engine/use it as receiver/context; QML binds framePublication.revision. Provider's original thread-safe image transport stays unchanged. This works independently of whether the public Qt provider inherits QObject; no new dependency floor or changed external module/process boundary.

All genuine initial-red/next-blue/previous-red and existing text/keyboard/clipboard assertions remain unchanged. The validefa4 displayed-red failure remains the old control. Strict7/all8/source/pixel/gates remain unrun for this exact repair; installed R18 and full scope are unchanged.
