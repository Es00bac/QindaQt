// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_service/vban_store.h>

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

#include <utility>

namespace QindaQt::Audio
{
namespace {

constexpr qint64 kMaxDocumentBytes = 64 * 1024;

QList<VbanStream> parseDefinitions(const QJsonObject &document)
{
    QList<VbanStream> streams;
    const auto append = [&streams](const QJsonValue &entry, bool outgoing) {
        if (!entry.isObject()) return;
        const QJsonObject object = entry.toObject();
        VbanStream stream;
        stream.outgoing = outgoing;
        stream.name = object.value(QStringLiteral("name")).toString();
        stream.busId = outgoing ? object.value(QStringLiteral("bus")).toString() : QString();
        stream.host = outgoing ? object.value(QStringLiteral("host")).toString()
                               : object.value(QStringLiteral("sourceHost")).toString();
        stream.outputNodeName = outgoing ? QString()
            : object.value(QStringLiteral("outputNodeName")).toString();
        const QJsonValue port = object.value(QStringLiteral("port"));
        if (port.isUndefined()) stream.port = 6980;
        else if (port.isDouble() && port.toInteger(-1) >= 1 && port.toInteger(-1) <= 65535)
            stream.port = static_cast<quint32>(port.toInteger());
        else return;
        if (!validateVbanDefinition(stream).accepted) return;
        for (const VbanStream &existing : streams) {
            if (existing.name == stream.name
                || (!stream.outgoing && !existing.outgoing && existing.port == stream.port)) return;
        }
        if (streams.size() < kMaxVbanStreams) streams.append(std::move(stream));
    };
    for (const QJsonValue &entry : document.value(QStringLiteral("outgoing")).toArray())
        append(entry, true);
    for (const QJsonValue &entry : document.value(QStringLiteral("incoming")).toArray())
        append(entry, false);
    return streams;
}

bool readDocument(const QString &path, QJsonObject *document, QString *reason)
{
    QFile file(path);
    if (!file.exists()) {
        *document = {};
        return true;
    }
    if (file.size() > kMaxDocumentBytes || !file.open(QIODevice::ReadOnly)) {
        if (reason) *reason = QStringLiteral("invalid-vban-document");
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(file.read(kMaxDocumentBytes + 1),
                                                           &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        if (reason) *reason = QStringLiteral("invalid-vban-document");
        return false;
    }
    *document = parsed.object();
    return true;
}

bool writeDefinitions(const QString &path, const QList<VbanStream> &streams,
                      QString *reason)
{
    QJsonArray outgoing;
    QJsonArray incoming;
    for (const VbanStream &stream : streams) {
        if (stream.outgoing) {
            outgoing.append(QJsonObject{{QStringLiteral("name"), stream.name},
                                        {QStringLiteral("bus"), stream.busId},
                                        {QStringLiteral("host"), stream.host},
                                        {QStringLiteral("port"), static_cast<int>(stream.port)}});
        } else {
            incoming.append(QJsonObject{{QStringLiteral("name"), stream.name},
                                        {QStringLiteral("sourceHost"), stream.host},
                                        {QStringLiteral("outputNodeName"), stream.outputNodeName},
                                        {QStringLiteral("port"), static_cast<int>(stream.port)}});
        }
    }
    const QByteArray data = QJsonDocument(QJsonObject{{QStringLiteral("outgoing"), outgoing},
                                                      {QStringLiteral("incoming"), incoming}})
                                .toJson(QJsonDocument::Indented);
    if (data.size() > kMaxDocumentBytes
        || !QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (reason) *reason = QStringLiteral("storage-failed");
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()
        || !file.commit()) {
        if (reason) *reason = QStringLiteral("storage-failed");
        return false;
    }
    return true;
}

} // namespace

VbanStore::VbanStore(QString path)
    : m_path(std::move(path))
{
}

QString VbanStore::defaultPath()
{
    const QByteArray override = qgetenv("QINDAQT_AUDIO_VBAN_PATH");
    if (!override.isEmpty()) return QString::fromUtf8(override);
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("qindaqt/audio-vban.json"));
}

QList<VbanStream> VbanStore::load() const
{
    QJsonObject document;
    return readDocument(m_path, &document, nullptr) ? parseDefinitions(document)
                                                   : QList<VbanStream>{};
}

bool VbanStore::upsert(const VbanStream &definition, QString *reasonCode) const
{
    if (!validateVbanDefinition(definition).accepted || definition.enabled
        || definition.active) {
        if (reasonCode) *reasonCode = QStringLiteral("invalid-vban-stream");
        return false;
    }
    QJsonObject document;
    if (!readDocument(m_path, &document, reasonCode)) return false;
    QList<VbanStream> streams = parseDefinitions(document);
    for (const VbanStream &stream : streams) {
        if (stream.name != definition.name && !definition.outgoing
            && !stream.outgoing && stream.port == definition.port) {
            if (reasonCode) *reasonCode = QStringLiteral("vban-port-in-use");
            return false;
        }
    }
    for (VbanStream &stream : streams) {
        if (stream.name == definition.name) {
            stream = definition;
            return writeDefinitions(m_path, streams, reasonCode);
        }
    }
    if (streams.size() >= kMaxVbanStreams) {
        if (reasonCode) *reasonCode = QStringLiteral("too-many-vban-streams");
        return false;
    }
    streams.append(definition);
    return writeDefinitions(m_path, streams, reasonCode);
}

bool VbanStore::remove(const QString &name, QString *reasonCode) const
{
    QJsonObject document;
    if (!readDocument(m_path, &document, reasonCode)) return false;
    QList<VbanStream> streams = parseDefinitions(document);
    for (qsizetype i = 0; i < streams.size(); ++i) {
        if (streams.at(i).name == name) {
            streams.removeAt(i);
            return writeDefinitions(m_path, streams, reasonCode);
        }
    }
    if (reasonCode) *reasonCode = QStringLiteral("unknown-vban-stream");
    return false;
}

} // namespace QindaQt::Audio
