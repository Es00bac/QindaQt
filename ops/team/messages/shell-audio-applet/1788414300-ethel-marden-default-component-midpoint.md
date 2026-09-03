# Default-component closure material finding

- Worker: Ethel Marden (`ethel-marden`)
- Exact repair base: `d47e2e92f4d44df110714c47998d44ae825ac9b3`
- Timestamp: `2026-09-02T23:45:00-06:00`

The new component-isolation runner is a demonstrated negative control: against
the unrepaired generated Debug install graph it exited 1 because the isolated
`QindaQt` stage lacked `lib64/libqindaqt_controls_qml.so`. The repair now names
`QindaQt` explicitly on the production shell install and adds Controls plus the
Tokens sibling to that component. The runner also parses the shell CMake rules
and rejects an unlisted shell-carrying component, so adding a fifth component
cannot silently escape the four-stage launch matrix. Final Debug and Release
executions each passed 1/1 after independently staging `QindaQt`,
`AudioAppletRuntime`, `BluetoothAppletRuntime`, and `PowerAppletRuntime`.
