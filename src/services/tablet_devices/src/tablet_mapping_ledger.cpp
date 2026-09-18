// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_mapping_ledger.h>

#include <qindaqt/services/tablet_devices/tablet_device_port.h>

namespace QindaQt::Services::TabletDevices {
namespace {

constexpr int MaxGroups = 32;
constexpr int MaxTextLength = 256;

bool boundedText(const QVariant &value, QString *out) {
    if (value.typeId() != QMetaType::QString) {
        return false;
    }
    const QString text = value.toString();
    if (text.size() > MaxTextLength) {
        return false;
    }
    *out = text;
    return true;
}

} // namespace

QString tabletMapChoiceName(TabletMapChoice choice) {
    switch (choice) {
    case TabletMapChoice::NamedOutput:
        return QStringLiteral("output");
    case TabletMapChoice::EntireWorkspace:
        return QStringLiteral("workspace");
    case TabletMapChoice::FollowActiveScreen:
        break;
    }
    return QStringLiteral("follow");
}

TabletMapChoice tabletMapChoiceFromString(const QString &text, bool *ok) {
    if (ok != nullptr) {
        *ok = true;
    }
    if (text == QLatin1String("output")) {
        return TabletMapChoice::NamedOutput;
    }
    if (text == QLatin1String("workspace")) {
        return TabletMapChoice::EntireWorkspace;
    }
    if (text == QLatin1String("follow")) {
        return TabletMapChoice::FollowActiveScreen;
    }
    if (ok != nullptr) {
        *ok = false;
    }
    return TabletMapChoice::FollowActiveScreen;
}

TabletMappingLedger
TabletMappingLedger::fromVariantMap(const QVariantMap &document) {
    TabletMappingLedger ledger;
    for (auto it = document.constBegin(); it != document.constEnd(); ++it) {
        if (ledger.m_records.size() >= MaxGroups) {
            break;
        }
        if (it.key().isEmpty() || it.key().size() > MaxTextLength ||
            it.value().typeId() != QMetaType::QVariantMap) {
            continue;
        }
        // AGENT-GUARD: a record written under KWin's deviceGroupId is keyed
        // on a pointer address and can never match a live device again.
        // Keeping it would leave a permanent phantom in the ledger and in
        // the Settings list, so a migration drops it; the tablet is then
        // treated as new and announces once more, which is the only visible
        // cost.
        if (isLegacyPointerHashIdentity(it.key())) {
            continue;
        }
        const QVariantMap entry = it.value().toMap();
        QString choiceText;
        if (!boundedText(entry.value(QStringLiteral("choice")), &choiceText)) {
            continue;
        }
        bool choiceOk = false;
        const TabletMapChoice choice =
            tabletMapChoiceFromString(choiceText, &choiceOk);
        if (!choiceOk) {
            continue;
        }
        TabletMappingRecord record;
        record.choice = choice;
        if (entry.contains(QStringLiteral("outputName")) &&
            !boundedText(entry.value(QStringLiteral("outputName")),
                         &record.outputName)) {
            continue;
        }
        if (entry.contains(QStringLiteral("deviceName")) &&
            !boundedText(entry.value(QStringLiteral("deviceName")),
                         &record.deviceName)) {
            continue;
        }
        // AGENT-GUARD: A named-output record with no output name would make
        // the policy write an empty outputName and silently hand the pen
        // back to the active screen. Drop it instead.
        if (record.choice == TabletMapChoice::NamedOutput &&
            record.outputName.isEmpty()) {
            continue;
        }
        const QVariant userChosen = entry.value(QStringLiteral("userChosen"));
        const QVariant announced = entry.value(QStringLiteral("announced"));
        if ((userChosen.isValid() && userChosen.typeId() != QMetaType::Bool) ||
            (announced.isValid() && announced.typeId() != QMetaType::Bool)) {
            continue;
        }
        record.userChosen = userChosen.toBool();
        record.announced = announced.toBool();
        ledger.m_records.insert(it.key(), record);
    }
    return ledger;
}

QVariantMap TabletMappingLedger::toVariantMap() const {
    QVariantMap document;
    for (auto it = m_records.constBegin(); it != m_records.constEnd(); ++it) {
        QVariantMap entry{
            {QStringLiteral("choice"), tabletMapChoiceName(it.value().choice)},
            {QStringLiteral("userChosen"), it.value().userChosen},
            {QStringLiteral("announced"), it.value().announced},
        };
        if (!it.value().outputName.isEmpty()) {
            entry.insert(QStringLiteral("outputName"), it.value().outputName);
        }
        if (!it.value().deviceName.isEmpty()) {
            entry.insert(QStringLiteral("deviceName"), it.value().deviceName);
        }
        document.insert(it.key(), entry);
    }
    return document;
}

bool TabletMappingLedger::contains(const QString &deviceGroupId) const {
    return m_records.contains(deviceGroupId);
}

TabletMappingRecord
TabletMappingLedger::record(const QString &deviceGroupId) const {
    return m_records.value(deviceGroupId);
}

void TabletMappingLedger::setRecord(const QString &deviceGroupId,
                                    const TabletMappingRecord &record) {
    if (deviceGroupId.isEmpty() || deviceGroupId.size() > MaxTextLength) {
        return;
    }
    if (!m_records.contains(deviceGroupId) &&
        m_records.size() >= MaxGroups) {
        return;
    }
    m_records.insert(deviceGroupId, record);
}

void TabletMappingLedger::remove(const QString &deviceGroupId) {
    m_records.remove(deviceGroupId);
}

QStringList TabletMappingLedger::groupIds() const {
    QStringList ids = m_records.keys();
    ids.sort();
    return ids;
}

} // namespace QindaQt::Services::TabletDevices
