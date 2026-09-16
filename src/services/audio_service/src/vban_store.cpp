// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_service/vban_store.h>

#include <qindaqt/services/audio_protocol/audio_limits.h>
#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>

#include <utility>

namespace QindaQt::Audio
{
namespace {

constexpr qint64 kMaxDocumentBytes = 64 * 1024;

[[nodiscard]] bool hostIsPlain(const QString &host)
{
    // A host name or literal address: letters, digits, dots, dashes, colons.
    // Nothing that could be read as a path or an option.
    for (const QChar character : host) {
        if (!(character.isLetterOrNumber() || character == QLatin1Char('.')
              || character == QLatin1Char('-') || character == QLatin1Char(':'))) {
            return false;
        }
    }
    return !host.isEmpty();
}

[[nodiscard]] bool nameIsPlain(const QString &name)
{
    for (const QChar character : name) {
        if (character.unicode() < 0x20 || character.unicode() > 0x7E) {
            return false;
        }
    }
    return !name.isEmpty() && name.toUtf8().size() <= kMaxVbanNameUtf8Bytes;
}

} // namespace

VbanStore::VbanStore(QString path)
    : m_path(std::move(path))
{
}

QString VbanStore::defaultPath()
{
    const QByteArray override = qgetenv("QINDAQT_AUDIO_VBAN_PATH");
    if (!override.isEmpty()) {
        return QString::fromUtf8(override);
    }
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("qindaqt/audio-vban.json"));
}

QList<VbanStream> VbanStore::load() const
{
    QList<VbanStream> streams;
    QFile file(m_path);
    if (!file.exists() || file.size() > kMaxDocumentBytes || !file.open(QIODevice::ReadOnly)) {
        return streams;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.read(kMaxDocumentBytes));
    if (!document.isObject()) {
        return streams;
    }
    const auto add = [&streams](VbanStream stream) {
        for (const VbanStream &existing : streams) {
            if (existing.name == stream.name) {
                return;
            }
        }
        if (streams.size() < kMaxVbanStreams) {
            streams.append(std::move(stream));
        }
    };
    for (const QJsonValue &entry : document.object().value(QStringLiteral("outgoing")).toArray()) {
        const QJsonObject object = entry.toObject();
        VbanStream stream;
        stream.outgoing = true;
        stream.name = object.value(QStringLiteral("name")).toString();
        stream.busId = object.value(QStringLiteral("bus")).toString();
        stream.host = object.value(QStringLiteral("host")).toString();
        const int port = object.value(QStringLiteral("port")).toInt(6980);
        if (!nameIsPlain(stream.name) || !isBoundedText(stream.busId, kMaxConsoleIdUtf8Bytes)
            || stream.busId.isEmpty() || !hostIsPlain(stream.host)
            || stream.host.toUtf8().size() > kMaxVbanHostUtf8Bytes || port <= 0 || port > 65535) {
            continue;
        }
        stream.port = static_cast<quint32>(port);
        add(stream);
    }
    for (const QJsonValue &entry : document.object().value(QStringLiteral("incoming")).toArray()) {
        const QJsonObject object = entry.toObject();
        VbanStream stream;
        stream.outgoing = false;
        stream.name = object.value(QStringLiteral("name")).toString();
        const int port = object.value(QStringLiteral("port")).toInt(6980);
        if (!nameIsPlain(stream.name) || port <= 0 || port > 65535) {
            continue;
        }
        stream.port = static_cast<quint32>(port);
        add(stream);
    }
    return streams;
}

} // namespace QindaQt::Audio
