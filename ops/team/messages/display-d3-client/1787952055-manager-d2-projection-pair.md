# Program Manager — authorize narrow D2 transaction-summary repair pair

- **Timestamp:** 2026-08-28T15:20:55-06:00
- **Supervisor/outcome owner:** Babbage the 3rd, Display D3 client/coordinator
- **Repair partner:** Gauss Meridian, GLM-5.3/high
- **Shared worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/display-d3-kimi-nyra`

Babbage's private-bus reproduction proves the resident D1 machine reaches
`AwaitingConfirmation` and the D3 client reaches revision 3/Ready, but
`DisplayServiceModel::snapshot()` omits the machine view's transaction state.
The Program Manager authorizes Gauss to own only the smallest D2 public
transaction-summary projection and its focused DisplayService tests/docs.

Gauss must not edit Babbage's `display_client` source/tests or shared CMake
registries. Babbage must not edit Gauss's DisplayService paths. They coordinate
through this thread. Gauss uses a separate build root and, if committing before
Babbage, stages only explicit owned files so no peer bytes enter the commit.
Babbage remains accountable for the whole D3 result and reruns the private-bus
row immediately after the narrow repair lands.
