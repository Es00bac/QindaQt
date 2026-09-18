// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_mapping_store.h>

#include <QVariant>

namespace QindaQt::Services::TabletDevices {
namespace {
constexpr auto kKey = "input.tabletMappings";
}

TabletMappingStore::TabletMappingStore(QObject *parent) : QObject(parent) {}

TabletMappingStore::~TabletMappingStore() = default;

bool TabletMappingStore::recordChoice(const QString &identity,
                                      TabletMapChoice choice,
                                      const QString &outputName,
                                      bool userChosen,
                                      const QString &deviceName) {
    if (identity.isEmpty()) {
        // A device with nothing stable to key on cannot be remembered, and
        // the caller must not believe a decision was recorded.
        return false;
    }
    TabletMappingLedger next = ledger();
    TabletMappingRecord record = next.record(identity);
    record.choice = choice;
    record.outputName =
        choice == TabletMapChoice::NamedOutput ? outputName : QString();
    // AGENT-GUARD: userChosen only ever goes false -> true. An automatic
    // pass must never clear a decision the user made.
    record.userChosen = record.userChosen || userChosen;
    record.announced = true;
    if (!deviceName.isEmpty()) {
        record.deviceName = deviceName;
    }
    next.setRecord(identity, record);
    return save(next);
}

const QStringList &Settings1TabletMappings::scopedKey() {
    static const QStringList keys{QLatin1String(kKey)};
    return keys;
}

Settings1TabletMappings::Settings1TabletMappings(
    SettingsClient::SettingsClient &client, QObject *parent)
    : TabletMappingStore(parent), m_client(client) {
    connect(&m_client, &SettingsClient::SettingsClient::snapshotChanged, this,
            &Settings1TabletMappings::onSnapshotChanged);
    connect(&m_client, &SettingsClient::SettingsClient::commitFinished, this,
            &Settings1TabletMappings::onCommitFinished);
    connect(&m_client, &SettingsClient::SettingsClient::commitUncertain, this,
            [this](const QString &) { onCommitFinished(); });
    onSnapshotChanged();
}

Settings1TabletMappings::~Settings1TabletMappings() = default;

bool Settings1TabletMappings::save(const TabletMappingLedger &ledger) {
    // The local copy moves first so a pass that reads back mid-write sees
    // what it just decided; the authority's confirmation replaces it.
    m_ledger = ledger;
    if (m_client.writeInFlight()) {
        // Coalesce rather than fail: the newest ledger already contains every
        // earlier decision, so only it needs to reach Settings1.
        m_pending = ledger;
        return true;
    }
    QString error;
    return m_client.setUserValue(QLatin1String(kKey),
                                 QVariant(ledger.toVariantMap()), &error);
}

void Settings1TabletMappings::onCommitFinished() { flushPending(); }

void Settings1TabletMappings::flushPending() {
    if (!m_pending.has_value() || m_client.writeInFlight()) {
        return;
    }
    const TabletMappingLedger next = *m_pending;
    m_pending.reset();
    QString error;
    if (!m_client.setUserValue(QLatin1String(kKey),
                               QVariant(next.toVariantMap()), &error)) {
        // The write could not even be dispatched. Republishing what the
        // authority confirms is the only honest recovery; the next decision
        // will try again.
        Q_EMIT ledgerChanged();
    }
}

void Settings1TabletMappings::onSnapshotChanged() {
    const auto &snapshot = m_client.snapshot();
    if (!snapshot.has_value()) {
        // No confirmed owner yet. Staying unloaded is deliberate: nothing
        // must map anything before it can see a recorded user choice.
        return;
    }
    const QVariant value = snapshot->values.value(QLatin1String(kKey));
    const TabletMappingLedger next =
        value.typeId() == QMetaType::QVariantMap
            ? TabletMappingLedger::fromVariantMap(value.toMap())
            : TabletMappingLedger{};
    const bool wasLoaded = m_loaded;
    m_loaded = true;
    if (wasLoaded && next == m_ledger) {
        return;
    }
    m_ledger = next;
    Q_EMIT ledgerChanged();
}

} // namespace QindaQt::Services::TabletDevices
