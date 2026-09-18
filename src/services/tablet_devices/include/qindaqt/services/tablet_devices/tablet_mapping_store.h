// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/tablet_devices/tablet_mapping_ledger.h>

#include <qindaqt/services/settings_client/settings_client.h>

#include <QObject>
#include <QStringList>

#include <optional>

namespace QindaQt::Services::TabletDevices {

// The remembered per-device mapping decisions.
//
// AGENT-CONTRACT: This seam and its Settings1 implementation live in the
// shared library on purpose. The session process decides mappings and the
// Settings route records what the user chose; both must write the SAME
// ledger through one implementation, or a choice made in Settings would be
// re-decided by the session a moment later and the route would read as doing
// nothing.
class TabletMappingStore : public QObject {
    Q_OBJECT
public:
    explicit TabletMappingStore(QObject *parent = nullptr);
    ~TabletMappingStore() override;

    // False until a real document has been read at least once.
    [[nodiscard]] virtual bool isLoaded() const = 0;
    [[nodiscard]] virtual TabletMappingLedger ledger() const = 0;
    // Persists the whole ledger. Returns false when the write could not even
    // be dispatched; a write that is merely uncertain returns true and the
    // store republishes whatever the authority confirms.
    virtual bool save(const TabletMappingLedger &ledger) = 0;

    // Records one device's choice without disturbing the others. Both
    // writers use this rather than read-modify-write on their own copy.
    bool recordChoice(const QString &identity, TabletMapChoice choice,
                      const QString &outputName, bool userChosen,
                      const QString &deviceName);

Q_SIGNALS:
    void ledgerChanged();
};

// Production truth: one purpose-scoped Settings1 client reading and writing
// only `input.tabletMappings`.
//
// AGENT-GUARD: Settings1 rejects a whole snapshot on one unknown key
// (ADR-0126), so the injected client must be scoped to exactly this key and
// the resident settings service must already know it from
// data/settings/schema-v2.json.
class Settings1TabletMappings final : public TabletMappingStore {
    Q_OBJECT
public:
    static const QStringList &scopedKey();

    explicit Settings1TabletMappings(
        SettingsClient::SettingsClient &client, QObject *parent = nullptr);
    ~Settings1TabletMappings() override;

    Settings1TabletMappings(const Settings1TabletMappings &) = delete;
    Settings1TabletMappings &operator=(const Settings1TabletMappings &) = delete;

    [[nodiscard]] bool isLoaded() const override { return m_loaded; }
    [[nodiscard]] TabletMappingLedger ledger() const override {
        return m_ledger;
    }
    bool save(const TabletMappingLedger &ledger) override;

private Q_SLOTS:
    void onSnapshotChanged();
    void onCommitFinished();

private:
    void flushPending();

    SettingsClient::SettingsClient &m_client;
    TabletMappingLedger m_ledger;
    // AGENT-GUARD: Settings1 allows one write at a time. Two devices decided
    // in the same pass would make the second write fail, and the caller
    // would tell the user their choice was not remembered when it simply had
    // not been sent yet. The newest ledger is held here and written when the
    // slot frees; only the newest is ever sent, because it already contains
    // every earlier decision.
    std::optional<TabletMappingLedger> m_pending;
    bool m_loaded = false;
};

} // namespace QindaQt::Services::TabletDevices
