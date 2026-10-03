# Review finding: GlobalShortcuts optional request token

Candidate `8f74faff5bd92bdfa1600cd0163b0e0b3f612fef` must not require
`handle_token` on CreateSession or BindShortcuts. xdg-desktop-portal 1.20.4
accepts and forwards it only when supplied by the client; its request path
uses a fallback token otherwise. `global_shortcuts_adaptor.cpp:21-30` rejected
both valid empty-token cases, and the native bridge test at
`tst_native_bridge.cpp:120-135` would fail. The implementer is repairing the
option contract and focused fixture before this review can accept a new exact
commit. The compositor peer and keyring fanout source did not show another
blocking issue in this pass.
