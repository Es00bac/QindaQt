// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_mapping_ledger.h>

#include <qindaqt/services/tablet_devices/tablet_device_port.h>
#include <qindaqt/services/tablet_devices/tablet_geometry.h>

#include <array>
#include <cmath>
#include <cstddef>

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

// AGENT-NOTE: Settings1 canonicalizes numbers on the wire: an integral
// double comes back as qint64 and only a fractional one as double
// (settings_protocol settings_wire_decode.cpp). A stored [0, 0, 1, 1] is
// therefore four integers. Any numeric type counts; a bool or a string that
// happens to convert does not.
bool numberOf(const QVariant &value, double *out) {
    switch (value.typeId()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Float:
    case QMetaType::Double: {
        const double number = value.toDouble();
        if (!std::isfinite(number)) {
            return false;
        }
        *out = number;
        return true;
    }
    default:
        return false;
    }
}

std::optional<Rotation> recordedRotation(const QVariant &value) {
    double degrees = 0.0;
    if (!numberOf(value, &degrees) || std::trunc(degrees) != degrees ||
        degrees < 0.0 || degrees > 270.0) {
        return std::nullopt;
    }
    return rotationFromDegrees(static_cast<int>(degrees));
}

std::optional<TabletArea> recordedArea(const QVariant &value) {
    if (value.typeId() != QMetaType::QVariantList) {
        return std::nullopt;
    }
    const QVariantList parts = value.toList();
    if (parts.size() != 4) {
        return std::nullopt;
    }
    std::array<double, 4> members{};
    for (std::size_t index = 0; index < members.size(); ++index) {
        if (!numberOf(parts.at(static_cast<qsizetype>(index)),
                      &members[index])) {
            return std::nullopt;
        }
    }
    // An intent that is no longer a usable area was not written by this code;
    // dropping it lets the planner adopt the device's own value again.
    return normalizedArea(
        TabletArea{members[0], members[1], members[2], members[3]});
}

// Members that are absent or malformed stay unset; the rest of the record,
// including its mapping decision, is kept either way.
TabletPlacementIntent placementFrom(const QVariantMap &entry) {
    TabletPlacementIntent placement;
    placement.rotation = recordedRotation(entry.value(QStringLiteral("rotation")));
    placement.inputArea = recordedArea(entry.value(QStringLiteral("inputArea")));
    placement.outputArea =
        recordedArea(entry.value(QStringLiteral("outputArea")));
    return placement;
}

void insertPlacement(const TabletPlacementIntent &placement,
                     QVariantMap *entry) {
    if (placement.rotation.has_value()) {
        entry->insert(QStringLiteral("rotation"),
                      rotationDegrees(*placement.rotation));
    }
    if (placement.inputArea.has_value()) {
        entry->insert(QStringLiteral("inputArea"),
                      placement.inputArea->toVariantList());
    }
    if (placement.outputArea.has_value()) {
        entry->insert(QStringLiteral("outputArea"),
                      placement.outputArea->toVariantList());
    }
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
        record.placement = placementFrom(entry);
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
        insertPlacement(it.value().placement, &entry);
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
