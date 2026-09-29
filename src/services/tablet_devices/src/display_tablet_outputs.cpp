// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/display_tablet_outputs.h>

#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_client/qt_display_transport.h>

#include <QDBusConnection>
#include <QPointF>
#include <QSizeF>

namespace QindaQt::Services::TabletDevices {

Rotation rotationForDisplayTransform(Display::Transform transform) noexcept {
    switch (transform) {
    case Display::Transform::Rotate90:
    case Display::Transform::FlipX90:
        return Rotation::Cw90;
    case Display::Transform::Rotate180:
    case Display::Transform::FlipX180:
        return Rotation::Cw180;
    case Display::Transform::Rotate270:
    case Display::Transform::FlipX270:
        return Rotation::Cw270;
    case Display::Transform::Normal:
    case Display::Transform::FlipX:
        break;
    }
    return Rotation::None;
}

QList<TabletOutputCandidate>
withDisplayRotations(QList<TabletOutputCandidate> screens,
                     const std::optional<Display::Snapshot> &snapshot) {
    if (!snapshot.has_value()) {
        return screens;
    }
    for (TabletOutputCandidate &screen : screens) {
        for (const Display::Output &output : snapshot->outputs) {
            if (!output.enabled || screen.connectorName.isEmpty() ||
                output.connectorName != screen.connectorName) {
                continue;
            }
            screen.rotation = rotationForDisplayTransform(output.transform);
            // Display1's logical size is already rotated and scaled, which
            // is the shape the user sees and the area editor draws.
            if (output.logicalSize.width() > 0 &&
                output.logicalSize.height() > 0) {
                screen.logicalGeometry =
                    QRectF(QPointF(output.position), QSizeF(output.logicalSize));
            }
            break;
        }
    }
    return screens;
}

DisplayRotationTabletOutputs::DisplayRotationTabletOutputs(
    TabletOutputInventory &screens, DisplayClient::Client &display,
    QObject *parent)
    : TabletOutputInventory(parent), m_screens(screens), m_display(display) {
    connectSources();
}

DisplayRotationTabletOutputs::DisplayRotationTabletOutputs(
    const QDBusConnection &bus, QObject *parent)
    : TabletOutputInventory(parent),
      m_ownedScreens(std::make_unique<ScreenTabletOutputs>()),
      m_screens(*m_ownedScreens),
      m_ownedTransport(std::make_unique<DisplayClient::QtDisplayTransport>(bus)),
      m_ownedClient(
          std::make_unique<DisplayClient::Client>(m_ownedTransport.get())),
      m_display(*m_ownedClient) {
    connectSources();
}

DisplayRotationTabletOutputs::~DisplayRotationTabletOutputs() {
    if (m_ownedClient != nullptr) {
        // Stop while the transport still exists, and without re-entering
        // this half-destroyed object through the client's last signals.
        disconnect(m_ownedClient.get(), nullptr, this, nullptr);
        m_ownedClient->stop();
    }
}

void DisplayRotationTabletOutputs::startDisplay() {
    if (m_ownedClient != nullptr) {
        m_ownedClient->start();
    }
}

void DisplayRotationTabletOutputs::connectSources() {
    m_published = outputs();
    connect(&m_screens, &TabletOutputInventory::outputsChanged, this,
            &DisplayRotationTabletOutputs::republish);
    connect(&m_display, &DisplayClient::Client::snapshotChanged, this,
            [this](const Display::Snapshot &) { republish(); });
    // Owner loss clears the client's snapshot and only reports a state
    // change; the rotations must turn unknown then, not stay stale.
    connect(&m_display, &DisplayClient::Client::stateChanged, this,
            [this](DisplayClient::ClientState, const QString &) {
                republish();
            });
}

QList<TabletOutputCandidate> DisplayRotationTabletOutputs::outputs() const {
    return withDisplayRotations(m_screens.outputs(), m_display.snapshot());
}

void DisplayRotationTabletOutputs::republish() {
    QList<TabletOutputCandidate> next = outputs();
    if (next == m_published) {
        return;
    }
    m_published = std::move(next);
    Q_EMIT outputsChanged();
}

} // namespace QindaQt::Services::TabletDevices
