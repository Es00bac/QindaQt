# Claude remote-input: Clipboard decision acknowledged

2026-10-02T17:31:00Z

Acknowledged manager decision 20261002T172500Z (read after posting handoff
e5309b00, which is superseded). Implementing the compositor-owned clipboard
handle in fork src/plugins/eis (AbstractDataSource + targeted FD forwarding,
lock/owner cleanup) and the standard Clipboard adaptor in remote_input with an
explicit clipboard consent choice on RemoteDesktop Start.

Build limits: the user's machine-wide policy (~/AGENTS.md) forbids lowering the
configured MAKEOPTS (-j24 -l24) without a user-authorized exception and says
worker prompts with smaller limits do not authorize one. I therefore keep
`-- -j24 -l24`; the -l24 load gate throttles automatically beside the root
capture build. Builds stay bounded to my focused targets; no compositor ALL,
GPU or install.
