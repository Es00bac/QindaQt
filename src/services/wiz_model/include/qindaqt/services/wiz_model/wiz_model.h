// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/wiz_protocol/wiz_messages.h>
#include <qindaqt/services/wiz_protocol/wiz_types.h>

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <optional>

namespace QindaQt::Wiz
{

// The observed truth about every luminaire this session knows, and the only
// place that decides when that truth has changed.
//
// AGENT-CONTRACT: this type performs no I/O and owns no timers. It is fed
// decoded datagrams and poll outcomes by the client and answers with a bounded
// snapshot. Keeping it free of sockets is what makes device lifecycle,
// reachability, and capability inference testable without a network.
//
// AGENT-GUARD: `revision` must advance if and only if a caller-visible value
// changed. The applet uses revision equality to decide that a dispatched
// operation has been confirmed by fresh device truth, so a gratuitous bump
// would report success that was never observed, and a missing bump would leave
// a confirmed operation pending forever.
class WizModel
{
public:
    WizModel();

    // Begins a new observation epoch. Inventory learned in an earlier epoch is
    // dropped: after a restart nothing has been confirmed by a live device.
    void start();
    void stop();

    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] quint64 epoch() const noexcept { return m_epoch; }
    [[nodiscard]] quint64 revision() const noexcept { return m_revision; }
    [[nodiscard]] std::optional<Device> device(const QString &mac) const;
    [[nodiscard]] QStringList knownMacs() const;
    // Routing information for a device, or an empty identity when unknown.
    [[nodiscard]] std::optional<DeviceIdentity> endpoint(const QString &mac) const;
    // The MAC of the one device known at `address`, or empty when nothing or
    // more than one device is there. Callers use it to attribute a reply the
    // firmware sent without its own identity.
    [[nodiscard]] QString deviceAtAddress(const QString &address) const;

    // Ingests one decoded datagram from `address`. Returns true when the
    // snapshot changed. A message without a usable MAC is ignored: identity
    // must come from the device, never from its current IP address.
    bool observe(const QString &address, quint16 port, const DecodedMessage &message);

    // A poll to this device went unanswered. Repeated misses walk the device
    // down the reachability ladder; it is never removed automatically, so a
    // stored label and its presets survive an outage.
    bool noteMissedPoll(const QString &mac);

    // Applies a label from stored configuration. An empty label restores the
    // name derived from the device's own model and MAC.
    bool applyStoredLabel(const QString &mac, const QString &label);

    bool setDiscovering(bool discovering);
    bool setAvailability(Availability availability, const QString &reasonCode,
                         const QString &diagnostic);

    // Drops a device from the inventory entirely. Used when the user forgets a
    // light; discovery may legitimately find it again.
    bool forget(const QString &mac);

private:
    // How many consecutive unanswered polls move a device to Stale, then to
    // Unreachable. Two polls of slack absorbs ordinary Wi-Fi datagram loss.
    static constexpr quint32 staleAfterMissedPolls = 2;
    static constexpr quint32 unreachableAfterMissedPolls = 4;

    bool commit(const QString &mac, const Device &updated);
    // The MAC of the one device already known at `address`, or empty when
    // nothing or more than one device is there.
    [[nodiscard]] QString soleDeviceAt(const QString &address) const;
    void refreshCapabilities(Device &device) const;

    quint64 m_epoch = 0;
    quint64 m_revision = 0;
    bool m_running = false;
    bool m_discovering = false;
    Availability m_availability = Availability::Starting;
    QString m_reasonCode;
    QString m_diagnostic;
    QHash<QString, Device> m_devices;
    // The last model configuration each device reported, kept beside the
    // device so capability inference can be redone when the module name
    // arrives in a later datagram than the model configuration.
    QHash<QString, ModelConfigPayload> m_modelConfigs;
    QHash<QString, QString> m_storedLabels;
};

} // namespace QindaQt::Wiz
