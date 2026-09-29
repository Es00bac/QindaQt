// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_service/latency_store.h>

#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>

#include <limits>
#include <utility>

namespace QindaQt::Audio
{
namespace
{

constexpr qint64 kMaxDocumentBytes = 128 * 1024;
constexpr int kDocumentVersion = 1;
constexpr qint64 kNotAnOffset = std::numeric_limits<qint64>::min();

[[nodiscard]] bool validNodeName(const QString &name)
{
    return !name.trimmed().isEmpty() && isBoundedText(name, kMaxNodeNameUtf8Bytes);
}

[[nodiscard]] bool validOffset(const qint64 offsetNs)
{
    return offsetNs >= kMinLatencyOffsetNs && offsetNs <= kMaxLatencyOffsetNs;
}

} // namespace

LatencyStore::LatencyStore(QString path)
    : m_path(std::move(path))
{
}

QString LatencyStore::defaultPath()
{
    // AGENT-GUARD: a probe or test service must never write the user's
    // document; the override keeps a second writer off the real file.
    const QByteArray override = qgetenv("QINDAQT_AUDIO_LATENCY_PATH");
    if (!override.isEmpty()) {
        return QString::fromUtf8(override);
    }
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("qindaqt/audio-latency.json"));
}

QMap<QString, qint64> LatencyStore::load() const
{
    QMap<QString, qint64> offsets;
    QFile file(m_path);
    if (!file.exists() || file.size() > kMaxDocumentBytes
        || !file.open(QIODevice::ReadOnly)) {
        return offsets;
    }
    // Bounded read, then parse: the file can grow after the size check.
    const QByteArray bytes = file.read(kMaxDocumentBytes + 1);
    if (bytes.size() > kMaxDocumentBytes) {
        return offsets;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return offsets;
    }
    const QJsonArray devices = document.object().value(QStringLiteral("devices")).toArray();
    for (const QJsonValue &entry : devices) {
        if (offsets.size() >= kMaxLatencyOffsets) {
            break;
        }
        const QJsonObject object = entry.toObject();
        const QString name = object.value(QStringLiteral("nodeName")).toString();
        const QJsonValue value = object.value(QStringLiteral("offsetNs"));
        // toInteger() answers the sentinel for a fraction; a string or a
        // missing value is not a number at all.
        const qint64 offsetNs = value.isDouble() ? value.toInteger(kNotAnOffset)
                                                 : kNotAnOffset;
        if (!validNodeName(name) || !validOffset(offsetNs) || offsets.contains(name)) {
            continue;
        }
        offsets.insert(name, offsetNs);
    }
    return offsets;
}

bool LatencyStore::set(const QString &nodeName, const qint64 offsetNs,
                       QString *reasonCode) const
{
    if (!validNodeName(nodeName) || !validOffset(offsetNs)) {
        *reasonCode = QStringLiteral("invalid-latency-offset");
        return false;
    }
    QMap<QString, qint64> offsets = load();
    if (!offsets.contains(nodeName) && offsets.size() >= kMaxLatencyOffsets) {
        *reasonCode = QStringLiteral("too-many-latency-offsets");
        return false;
    }
    offsets.insert(nodeName, offsetNs);
    QJsonArray devices;
    for (auto it = offsets.cbegin(); it != offsets.cend(); ++it) {
        devices.append(QJsonObject{{QStringLiteral("nodeName"), it.key()},
                                   {QStringLiteral("offsetNs"), it.value()}});
    }
    const QByteArray bytes = QJsonDocument(QJsonObject{
                                               {QStringLiteral("schemaVersion"), kDocumentVersion},
                                               {QStringLiteral("devices"), devices},
                                           })
                                 .toJson(QJsonDocument::Indented);
    // QSaveFile: the document is complete on disk or not replaced at all.
    QSaveFile file(m_path);
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())
        || !file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
        || !file.commit()) {
        *reasonCode = QStringLiteral("latency-store-unwritable");
        return false;
    }
    return true;
}

} // namespace QindaQt::Audio
