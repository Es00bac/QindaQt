# Independent exact native-lock review: ACCEPT

- Reviewer: ed-foreign-astra-20261007; different author from root.
- Exact source: 65bd6479e92f9ff0f769625f1436e15aa5c45167.
- Decision: ACCEPT the bounded transport error classification repair. This is not a whole lock, installed session or physical protection qualification.

The actual transport diff only preserves UnknownObject/UnknownInterface from an actual D-Bus ErrorMessage for the monitor's existing five delayed retries. All other errors and malformed replies remain generic failure. The monitor still requires current exact-owner admission, nonce receipt, serial/generation and reply checks; retry cannot outlive revocation. Static review found no blocking change to this authority.

I inspected the manager's actual transferred evidence at everyday-manager-20261007/.cache/ed-lock-manager-proof/{old-native-ctest,fixed-native-ctest}.log and its CMakeLists.txt. The harness uses identical new tests and substitutes only the transport source for the old comparison. Old transport: 11 Qt pass, 2 fail (late object publication and startup UnknownInterface), zero skipped; failing CTest exit. Fixed transport: 13/13 Qt; monitor 15/15 Qt; CTest 2/2 exit 0, zero skipped. The exact old failures correspond to the source repair. These are manager-run native results read independently, not a claim that I executed this harness. Host-bus blocking is the manager's execution report; the logs do not themselves print environment variables.

Hidden Wi-Fi independent ACCEPT is already durable at commit 62f133a24f73136f0d106edd1ea5831dc8153427, ops/team/messages/2026-10-07T231120Z-hidden-accept-copy-sentinel-reproduction.md: exact 14b83f50d3632739374f9e69255d8e6a8a2008d3 source review plus my seven focused CTests / 40 Qt rows passed under the granted private-fixture sublease; author's full 39/39 and 254 Qt logs separately inspected.

Requested next action: manager integrates the two reviewed exact fixes and runs the affected integrated gates. I continue the separately owned copier preservation repair source-only; root retains the compiler and private-fixture lease.
