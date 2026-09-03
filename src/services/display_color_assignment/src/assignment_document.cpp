// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_color_assignment/assignment_document.h>

#include <QtCore/QSet>

#include <algorithm>

namespace QindaQt::DisplayColor
{
namespace
{

constexpr auto FieldProfile = "profile";
constexpr auto FieldLineage = "lineage";

bool isLowercaseHexDigest(const QString &text)
{
    if (text.size() != Sha256HexLength) {
        return false;
    }
    for (const QChar ch : text) {
        const ushort u = ch.unicode();
        const bool hex = (u >= u'0' && u <= u'9') || (u >= u'a' && u <= u'f');
        if (!hex) {
            return false;
        }
    }
    return true;
}

} // namespace

AssignmentDocumentDecodeResult decodeAssignmentDocument(const QVariant &value)
{
    AssignmentDocumentDecodeResult result;
    if (!value.isValid() || value.isNull()) {
        result.reasonCode = QStringLiteral("absent-value");
        return result;
    }
    if (value.userType() != QMetaType::QVariantMap) {
        result.reasonCode = QStringLiteral("not-an-object");
        return result;
    }
    const QVariantMap map = value.toMap();
    if (map.size() > static_cast<int>(MaxOutputs)) {
        result.reasonCode = QStringLiteral("output-cap-exceeded");
        return result;
    }

    AssignmentDocument document;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        ColorAssignmentRecord record;
        record.outputStableId = it.key();
        if (!validateDisplayStableId(record.outputStableId)) {
            result.reasonCode = QStringLiteral("invalid-output-id");
            return result;
        }
        if (it.value().userType() != QMetaType::QVariantMap) {
            result.reasonCode = QStringLiteral("record-not-an-object");
            return result;
        }
        const QVariantMap recordMap = it.value().toMap();
        if (recordMap.size() != 2 || !recordMap.contains(QLatin1String(FieldProfile)) ||
            !recordMap.contains(QLatin1String(FieldLineage))) {
            result.reasonCode = QStringLiteral("record-field-set");
            return result;
        }
        const QVariant profileValue = recordMap.value(QLatin1String(FieldProfile));
        const QVariant lineageValue = recordMap.value(QLatin1String(FieldLineage));
        if (profileValue.userType() != QMetaType::QString ||
            lineageValue.userType() != QMetaType::QString) {
            result.reasonCode = QStringLiteral("record-field-type");
            return result;
        }
        record.profileId = profileValue.toString();
        if (!validateDisplayStableId(record.profileId)) {
            result.reasonCode = QStringLiteral("invalid-profile-id");
            return result;
        }
        const QString lineageHex = lineageValue.toString();
        if (!lineageHex.isEmpty()) {
            if (!isLowercaseHexDigest(lineageHex)) {
                result.reasonCode = QStringLiteral("invalid-lineage");
                return result;
            }
            record.lineageFingerprint = QByteArray::fromHex(lineageHex.toLatin1());
        }
        document.records.append(record);
    }

    std::sort(document.records.begin(), document.records.end(),
              [](const ColorAssignmentRecord &a, const ColorAssignmentRecord &b) {
                  return a.outputStableId < b.outputStableId;
              });
    result.ok = true;
    result.document = std::move(document);
    return result;
}

std::optional<QVariant> encodeAssignmentDocument(const AssignmentDocument &document)
{
    QVariantMap map;
    for (const ColorAssignmentRecord &record : document.records) {
        if (!validateDisplayStableId(record.outputStableId) ||
            !validateDisplayStableId(record.profileId)) {
            return std::nullopt;
        }
        if (record.lineageFingerprint.size() != Sha256DigestBytes &&
            !record.lineageFingerprint.isEmpty()) {
            return std::nullopt;
        }
        if (map.contains(record.outputStableId)) {
            return std::nullopt;
        }
        QVariantMap recordMap;
        recordMap.insert(QLatin1String(FieldProfile), record.profileId);
        recordMap.insert(QLatin1String(FieldLineage),
                         record.lineageFingerprint.isEmpty()
                             ? QString()
                             : QString::fromLatin1(record.lineageFingerprint.toHex()));
        map.insert(record.outputStableId, recordMap);
    }
    if (map.size() > static_cast<int>(MaxOutputs)) {
        return std::nullopt;
    }
    return QVariant(map);
}

ColorAssignmentDraftValidation validateColorAssignmentDraft(const ColorAssignmentDraft &draft)
{
    ColorAssignmentDraftValidation validation;
    QSet<QString> outputs;
    for (const ColorAssignmentDraftEntry &entry : draft.entries) {
        if (!validateDisplayStableId(entry.outputStableId)) {
            validation.reasonCode = QStringLiteral("invalid-output-id");
            return validation;
        }
        // AGENT-GUARD: One output may appear once per draft; two entries for
        // the same output would make the merged document depend on draft
        // entry order instead of a single deterministic intent.
        if (!outputs.contains(entry.outputStableId)) {
            if (outputs.size() >= static_cast<int>(MaxOutputs)) {
                validation.reasonCode = QStringLiteral("output-cap-exceeded");
                return validation;
            }
            outputs.insert(entry.outputStableId);
        } else {
            validation.reasonCode = QStringLiteral("duplicate-output");
            return validation;
        }
        if (entry.remove) {
            continue;
        }
        if (!validateDisplayStableId(entry.profileId)) {
            validation.reasonCode = QStringLiteral("invalid-profile-id");
            return validation;
        }
        if (entry.lineageFingerprint.size() != Sha256DigestBytes &&
            !entry.lineageFingerprint.isEmpty()) {
            validation.reasonCode = QStringLiteral("invalid-lineage");
            return validation;
        }
    }
    validation.ok = true;
    return validation;
}

ColorAssignmentApplyResult applyColorAssignmentDraft(const AssignmentDocument &document,
                                                     const ColorAssignmentDraft &draft)
{
    ColorAssignmentApplyResult result;
    const ColorAssignmentDraftValidation validation = validateColorAssignmentDraft(draft);
    if (!validation.ok) {
        result.reasonCode = validation.reasonCode;
        return result;
    }

    result.next = document;
    for (const ColorAssignmentDraftEntry &entry : draft.entries) {
        bool replaced = false;
        for (qsizetype i = 0; i < result.next.records.size(); ++i) {
            if (result.next.records.at(i).outputStableId != entry.outputStableId) {
                continue;
            }
            if (entry.remove) {
                result.next.records.removeAt(i);
            } else {
                ColorAssignmentRecord updated = result.next.records.at(i);
                updated.profileId = entry.profileId;
                updated.lineageFingerprint = entry.lineageFingerprint;
                result.next.records[i] = updated;
            }
            replaced = true;
            break;
        }
        if (!replaced && !entry.remove) {
            ColorAssignmentRecord record;
            record.outputStableId = entry.outputStableId;
            record.profileId = entry.profileId;
            record.lineageFingerprint = entry.lineageFingerprint;
            result.next.records.append(record);
        }
    }

    std::sort(result.next.records.begin(), result.next.records.end(),
              [](const ColorAssignmentRecord &a, const ColorAssignmentRecord &b) {
                  return a.outputStableId < b.outputStableId;
              });
    if (result.next.records.size() > static_cast<int>(MaxOutputs)) {
        result.ok = false;
        result.reasonCode = QStringLiteral("output-cap-exceeded");
        result.next = AssignmentDocument{};
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace QindaQt::DisplayColor
