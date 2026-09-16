// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/audio_service/console_store.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>

#include <utility>

namespace QindaQt::Audio
{

ConsoleStore::ConsoleStore(QString path)
    : m_path(std::move(path))
{
}

QString ConsoleStore::defaultPath()
{
    // AGENT-GUARD: a second service run against the same graph - a probe, a
    // test harness - must never share the user's document with the resident
    // one. Two writers on one file would interleave, and a probe's routing
    // would be restored into the user's console.
    const QByteArray override = qgetenv("QINDAQT_AUDIO_CONSOLE_PATH");
    if (!override.isEmpty()) {
        return QString::fromUtf8(override);
    }
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
        .filePath(QStringLiteral("qindaqt/audio-console.json"));
}

bool ConsoleStore::load(ConsoleModel &model) const
{
    QFile file(m_path);
    if (!file.exists() || file.size() > kMaxDocumentBytes
        || !file.open(QIODevice::ReadOnly)) {
        return false;
    }
    // AGENT-GUARD: bounded read, then parse. The size was checked before
    // opening, but a file can grow between the two calls.
    const QByteArray bytes = file.read(kMaxDocumentBytes + 1);
    if (bytes.size() > kMaxDocumentBytes) {
        return false;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    model.loadJson(document.object());
    return true;
}

bool ConsoleStore::save(const ConsoleModel &model)
{
    const QByteArray bytes = QJsonDocument(model.toJson()).toJson(QJsonDocument::Indented);
    if (bytes == m_lastWritten) {
        return true;
    }
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        return false;
    }
    // QSaveFile: the document is complete on disk or not there at all. A
    // crash mid-write leaves the previous console, never half of the new one.
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
        || !file.commit()) {
        return false;
    }
    m_lastWritten = bytes;
    return true;
}

} // namespace QindaQt::Audio
