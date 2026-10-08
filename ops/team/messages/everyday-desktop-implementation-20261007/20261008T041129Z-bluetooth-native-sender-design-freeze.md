# Bluetooth sender-preserving design freeze

- Time: 2026-10-08T04:11:29Z
- Actual implementation remains NEEDS_FIX after 26/29 main and 1/3 focused results. Product source unchanged from 814d; later fixture-only diagnostics remain preserved.
- Explicitly released all compiler/private-runtime resources to root. No native process or host action.
- Withdraw 638 broker-default trust premise following Platform exact70d850 review; installed eavesdrop rules defeat requested-only filtering.
- Proposed ADR0359 records the stronger native transport, owned same-bus session factory, GUID handshake pin, full owner-issued raw-caller delegation, real sender+serial verification, callback/teardown fences and required adversarial gates.
- Exact Portage archive SHA256 0ba2a1a4b16afe7bceb2c07e9ce99a8c2c3508e5dec290dbb643384bd6beb7e2; dbus-transport.c781–788 explicitly disconnects on supplied address GUID mismatch. Qt internalPointer alternative is documented as internal/implementation-defined and not silently adopted.
- Proposed added owning paths: public radio_service_session.h, private native_radio_wire/request_codec/session/port collaborators and native service/authority adapters under src/services/bluetooth_radio_helper; existing owning CMake/main/fixtures/ADR/wiki/SDK consumer. No private Keyring imports, broker policy, protocol Bluetooth1/2 or main-unit change.
- Request root/Platform exact contract decision before production expansion. All existing radio operation/platform/backend and Accepted copier/ED05 boundaries remain preserved.
