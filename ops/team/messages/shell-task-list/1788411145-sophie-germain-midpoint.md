# Sophie Germain midpoint — authenticated shell window actions

- Exact base: `d0b70ed80d9c6bf45b9d3b6219d1e11514514c4c`.
- ADR-0057 records the separate `org.qindaqt.CompositorShell1` boundary and its local-process threat model.
- The fail-closed controller, KWin credential/panel-owner/registry/executor adapters, five shell mutations, and exact-owner asynchronous client compile in the strict Debug build.
- Focused Debug contract, controller, client, and private-bus tests pass 4/4.
- Remaining work is the private virtual-KWin authenticated live row, owning wiki updates, Release verification, and static gates.
