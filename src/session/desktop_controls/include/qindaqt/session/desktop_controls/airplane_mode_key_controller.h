// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/network_client/network_client.h>

#include <QObject>

namespace QindaQt::Session::DesktopControls {

// XF86WLAN over the public NetworkClient. Toggles the Wi-Fi radio's
// software-enabled state.
//
// AGENT-CONTRACT: only the Wi-Fi radio is toggled. NetworkClient (like
// every other resident client in this codebase) admits one operation at a
// time; a second setRadio() call issued synchronously right after the
// first is rejected as busy, not queued. A same-target-state WWAN follow-up
// would need to wait for the Wi-Fi operation's completion signal before
// dispatching, which is real state-machine complexity this controller does
// not take on. Bluetooth is a separate public client and is not touched
// either. See docs/wiki/architecture/desktop-controls.md for both
// decisions.
//
// XF86RFKill has no Qt::Key mapping (absent from Qt's compiled keysym
// table; verified against the installed libQt6Gui.so) and cannot be bound
// through QShortcut/QAction/KGlobalAccel the way every other media key in
// this codebase is. Only XF86WLAN (Qt::Key_WLAN) is wired; a keyboard
// whose physical airplane key emits XF86RFKill instead produces no Qt key
// event at all and is a known, documented gap.
class AirplaneModeKeyController final : public QObject {
    Q_OBJECT

public:
    explicit AirplaneModeKeyController(Network::Client::NetworkClient &client,
                                       QObject *parent = nullptr);
    ~AirplaneModeKeyController() override;

    AirplaneModeKeyController(const AirplaneModeKeyController &) = delete;
    AirplaneModeKeyController &operator=(const AirplaneModeKeyController &) = delete;

    void toggleAirplaneMode();

Q_SIGNALS:
    // `airplaneModeOn` is the intended state after this trigger: true means
    // the Wi-Fi radio was just asked to disable.
    void airplaneModeFeedbackRequested(bool airplaneModeOn);
    void airplaneModeUnavailable(const QString &reasonCode);

private:
    Network::Client::NetworkClient &m_client;
};

} // namespace QindaQt::Session::DesktopControls
