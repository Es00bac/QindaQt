// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Services::TabletDevices {

// How a tablet's pointer reaches a screen.
enum class TabletMapChoice {
    // KWin's own default: the pen follows whichever output is active.
    FollowActiveScreen,
    // One named output owns the pen.
    NamedOutput,
    // The pen spans the whole workspace (KWin's mapToWorkspace).
    EntireWorkspace,
};

// One remembered decision for one device group (pen and pad of the same
// tablet share `deviceGroupId`).
//
// AGENT-CONTRACT: `userChosen` is the whole point of this record. Automatic
// mapping may write a decision once; once the user picks anything, automatic
// mapping never overrides it again, on this or any later plug-in.
struct TabletMappingRecord {
    TabletMapChoice choice = TabletMapChoice::FollowActiveScreen;
    QString outputName; // meaningful only for NamedOutput
    bool userChosen = false;
    // True once the arrival notification has been shown for this group, so a
    // re-plug of a known tablet stays silent.
    bool announced = false;
    // The device name at the time of the record, for diagnostics and for a
    // readable Settings list when the tablet is unplugged.
    QString deviceName;

    friend bool operator==(const TabletMappingRecord &,
                           const TabletMappingRecord &) = default;
};

// The whole ledger, as it is stored in one Settings1 object key.
//
// AGENT-NOTE: Settings1's schema is a closed list of keys (ADR-0126), so a
// key per device group cannot exist. The ledger is therefore one `object`
// value, `input.tabletMappings`, whose members are device group ids. Unknown
// members and malformed records are dropped on read rather than failing the
// whole document: a record this version cannot read must not cost the user
// the records it can.
class TabletMappingLedger final {
public:
    [[nodiscard]] static TabletMappingLedger fromVariantMap(
        const QVariantMap &document);
    [[nodiscard]] QVariantMap toVariantMap() const;

    [[nodiscard]] bool contains(const QString &deviceGroupId) const;
    [[nodiscard]] TabletMappingRecord record(const QString &deviceGroupId) const;
    void setRecord(const QString &deviceGroupId,
                   const TabletMappingRecord &record);
    void remove(const QString &deviceGroupId);

    [[nodiscard]] QStringList groupIds() const;
    [[nodiscard]] bool isEmpty() const { return m_records.isEmpty(); }
    [[nodiscard]] qsizetype size() const { return m_records.size(); }

    friend bool operator==(const TabletMappingLedger &a,
                           const TabletMappingLedger &b) {
        return a.m_records == b.m_records;
    }

private:
    QHash<QString, TabletMappingRecord> m_records;
};

// AGENT-GUARD: deliberately not named toString: QTest's genericToString
// finds a same-named free function by ADL and then fails to compile every
// QCOMPARE of this enum because the return type is not const char *.
[[nodiscard]] QString tabletMapChoiceName(TabletMapChoice choice);
[[nodiscard]] TabletMapChoice tabletMapChoiceFromString(const QString &text,
                                                        bool *ok = nullptr);

} // namespace QindaQt::Services::TabletDevices
