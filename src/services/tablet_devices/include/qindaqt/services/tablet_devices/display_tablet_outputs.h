// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/display_protocol/display_types.h>
#include <qindaqt/services/tablet_devices/tablet_output_inventory.h>

#include <QList>

#include <memory>
#include <optional>

class QDBusConnection;

namespace QindaQt::DisplayClient {
class Client;
class DisplayTransport;
} // namespace QindaQt::DisplayClient

namespace QindaQt::Services::TabletDevices {

// The rotation KWin applies to tablet input on an output whose Display1
// transform is `transform`.
//
// AGENT-NOTE: KWin's devicePointToGlobalPosition turns every FlipX* transform
// exactly like the matching Rotate* and never mirrors pen input (kwin-6.6.6
// src/backends/libinput/connection.cpp carries a TODO about it). A desk
// tablet's compensation must undo what KWin does to the pen, not what the
// transform does to pixels, so the flip is dropped here on purpose.
[[nodiscard]] Rotation
rotationForDisplayTransform(Display::Transform transform) noexcept;

// Pure join: each screen gains the rotation and logical geometry Display1
// publishes for the same connector. Without a snapshot, or for a connector the
// snapshot does not list as enabled, the rotation stays unknown and the
// geometry stays whatever the screen list said.
[[nodiscard]] QList<TabletOutputCandidate>
withDisplayRotations(QList<TabletOutputCandidate> screens,
                     const std::optional<Display::Snapshot> &snapshot);

// Output inventory decorated with what KWin does to tablet input on each
// output (ADR-0285).
//
// AGENT-NOTE: QScreen cannot answer this. Qt's Wayland screen derives
// orientation() from the wl_output transform AND whether the logical
// geometry is portrait, and ignores flipped transforms (qtbase 6.11
// qwaylandscreen.cpp toScreenOrientation), so a landscape panel turned 90°
// reads as InvertedLandscape. Display1 publishes the compositor's own
// transform per connector — the value KWin feeds devicePointToGlobalPosition.
// It is an existing session service, so this adds a client, not a process.
//
// Lifetime and threading: everything lives on this object's thread. The
// borrowing constructor needs `screens` and `display` to outlive it and leaves
// starting and stopping `display` to its owner. outputsChanged() fires only
// when the joined list actually changes.
class DisplayRotationTabletOutputs final : public TabletOutputInventory {
    Q_OBJECT
public:
    DisplayRotationTabletOutputs(TabletOutputInventory &screens,
                                 DisplayClient::Client &display,
                                 QObject *parent = nullptr);
    // Production: owns the process's QScreen list and a purpose-scoped
    // Display1 client on `bus`. Nothing touches the bus until startDisplay().
    explicit DisplayRotationTabletOutputs(const QDBusConnection &bus,
                                          QObject *parent = nullptr);
    ~DisplayRotationTabletOutputs() override;

    // Starts the owned Display1 client (asynchronously; until it answers every
    // rotation is unknown). A no-op for a borrowed client.
    void startDisplay();

    [[nodiscard]] QList<TabletOutputCandidate> outputs() const override;

private:
    void connectSources();
    void republish();

    // AGENT-GUARD: declaration order is destruction order in reverse. The
    // owned client must go before its transport (Display1 client contract),
    // and the references bind to the owned objects declared above them.
    std::unique_ptr<TabletOutputInventory> m_ownedScreens;
    TabletOutputInventory &m_screens;
    std::unique_ptr<DisplayClient::DisplayTransport> m_ownedTransport;
    std::unique_ptr<DisplayClient::Client> m_ownedClient;
    DisplayClient::Client &m_display;
    QList<TabletOutputCandidate> m_published;
};

} // namespace QindaQt::Services::TabletDevices
