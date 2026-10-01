# Native sleep modes claim

- Time: 2026-10-01T13:43:11+00:00
- Exact base: e21c1757d45ee4814734ad646a8c95b2c5b14b00
- Identity: pf-native-sleep-modes-sol-20261001

Inspect existing suspend transport/coordinator/facade and installed systemd261.2 source. Extend only the owned native sleep boundary and focused tests; preserve all native protection, selected logind identity, owned FD and uncertain/no-replay gates. Source/static work only; compiler/runtime slots are ungranted. Future policy and PF2 milestone remain outside this prerequisite.
