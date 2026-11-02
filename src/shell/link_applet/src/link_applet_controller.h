// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_client/link_client.h>
#include <qindaqt/shell/link_applet/link_applet_presentation.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <functional>

namespace QindaQt::Shell::LinkApplet {

// Shell-private adapter from the public LinkClient to bounded QML values
// (docs/wiki/shell/link-applet.md). Borrows a same-thread client owned by
// shell composition; the client must outlive the controller.
//
// AGENT-CONTRACT: `phone.read` false means `client` may be null (composition
// constructs no transport or client) and the controller presents permanently
// as Unavailable without touching it. `phone.control` false keeps the read
// view but makes ConfirmPairing, Ring and Unpair no-ops (`canControl` false).
//
// AGENT-NOTE: Link1 has no "pending pairing" property, only the
// PairingRequested signal, so a request raised before the shell started is
// not shown here (the service's own notification still carries it). The
// pending request is dropped when the service vanishes, the device list
// stops showing that phone as an unpaired connection, or the user decides.
class LinkAppletController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY modelChanged)
    Q_PROPERTY(QString stateText READ stateText NOTIFY modelChanged)
    Q_PROPERTY(QString iconColorRole READ iconColorRole NOTIFY modelChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY modelChanged)
    Q_PROPERTY(QString connectionText READ connectionText NOTIFY modelChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY modelChanged)
    Q_PROPERTY(QString batteryText READ batteryText NOTIFY modelChanged)
    Q_PROPERTY(bool charging READ charging NOTIFY modelChanged)
    Q_PROPERTY(QString networkText READ networkText NOTIFY modelChanged)
    Q_PROPERTY(QString lastSeenText READ lastSeenText NOTIFY modelChanged)
    Q_PROPERTY(bool canRing READ canRing NOTIFY modelChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY modelChanged)
    Q_PROPERTY(bool hasPairing READ hasPairing NOTIFY modelChanged)
    Q_PROPERTY(QString pairingName READ pairingName NOTIFY modelChanged)
    Q_PROPERTY(QString pairingModel READ pairingModel NOTIFY modelChanged)
    Q_PROPERTY(QString pairingCode READ pairingCode NOTIFY modelChanged)
    Q_PROPERTY(bool hasPhone READ hasPhone NOTIFY modelChanged)
    Q_PROPERTY(bool canControl READ canControl CONSTANT)
    Q_PROPERTY(bool canOpenApp READ canOpenApp NOTIFY canOpenAppChanged)

public:
    using Launch = std::function<bool()>;
    using Availability = std::function<bool()>;
    using Clock = std::function<qint64()>;

    LinkAppletController(Link::LinkClient *client, bool phoneReadGranted,
                         bool phoneControlGranted, QObject *parent = nullptr);
    ~LinkAppletController() override;

    [[nodiscard]] QString state() const { return toString(m_model.state); }
    [[nodiscard]] QString stateText() const { return m_model.stateText; }
    [[nodiscard]] QString iconColorRole() const { return m_model.iconColorRole; }
    [[nodiscard]] QString deviceName() const { return m_model.deviceName; }
    [[nodiscard]] QString connectionText() const { return m_model.connectionText; }
    [[nodiscard]] int batteryPercent() const noexcept { return m_model.batteryPercent; }
    [[nodiscard]] QString batteryText() const { return m_model.batteryText; }
    [[nodiscard]] bool charging() const noexcept { return m_model.charging; }
    [[nodiscard]] QString networkText() const { return m_model.networkText; }
    [[nodiscard]] QString lastSeenText() const { return m_model.lastSeenText; }
    [[nodiscard]] bool canRing() const noexcept { return m_model.canRing && m_control; }
    [[nodiscard]] int unreadCount() const noexcept { return m_model.unreadCount; }
    [[nodiscard]] bool hasPairing() const noexcept { return m_model.hasPairing; }
    [[nodiscard]] QString pairingName() const { return m_model.pairing.name; }
    [[nodiscard]] QString pairingModel() const { return m_model.pairing.model; }
    [[nodiscard]] QString pairingCode() const { return m_model.pairing.code; }
    [[nodiscard]] bool hasPhone() const noexcept { return !m_model.deviceId.isEmpty(); }
    [[nodiscard]] bool canControl() const noexcept { return m_control; }
    [[nodiscard]] bool canOpenApp() const noexcept { return m_canOpenApp; }
    [[nodiscard]] const LinkAppletModel &model() const noexcept { return m_model; }

    // Composition seams. `available` answers whether the QindaLink desktop
    // entry exists; `launch` starts it through the shell's launcher.
    void setAppLaunch(Availability available, Launch launch);
    void setClock(Clock clock);

    Q_INVOKABLE void refreshAppAvailability();
    Q_INVOKABLE void acceptPairing();
    Q_INVOKABLE void rejectPairing();
    Q_INVOKABLE void ringPhone();
    // Unpair is destructive; QML puts a confirmation in front of it.
    Q_INVOKABLE void unpairPhone();
    Q_INVOKABLE void openApp();

Q_SIGNALS:
    void modelChanged();
    void canOpenAppChanged();

private:
    void reproject();
    void onDevicesChanged();
    void onPairingRequested(const QString &id, const QString &name, const QString &model,
                            const QString &code);

    Link::LinkClient *m_client = nullptr;
    bool m_read = false;
    bool m_control = false;
    bool m_canOpenApp = false;
    std::optional<PendingPairing> m_pairing;
    LinkAppletModel m_model;
    Availability m_appAvailable;
    Launch m_appLaunch;
    Clock m_clock;
    QTimer m_refresh; // keeps "last seen" honest while offline
};

} // namespace QindaQt::Shell::LinkApplet
