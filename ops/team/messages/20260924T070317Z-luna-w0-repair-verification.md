# W0 repair verification

- Target build: passed with exit status 0 using `cmake --build build/dev --target qindaqt_network_secret_agent_controller_tests qindaqt_network_secret_agent_dbus_tests qindaqt_network_secret_agent_ip_config_tests qindaqt_network_secret_agent_lifetime_tests qindaqt-network-secret-agent -- -j8 -l20`.
- Secret-agent private-bus and process-lifetime matrix: 3 selected CTest cases repeated five times, 15/15 executions passed.
- Broader network and Settings Network selection: 39/39 tests passed, exit status 0.
- Documentation validation: `./tools/validate-docs` passed, 388 Markdown documents and `mkdocs.yml` navigation validated.
- `git diff --check` passed.

The regressions cover the VPN-shaped `a{ss}` value under both admitted and unsupported-setting/refused requests, exact aggregate and first-excess limits for `aay` and `a(ayuay)`, UTF-8 sizing edge cases, and scrubbing of detached byte buffers and recursive map keys/values.
